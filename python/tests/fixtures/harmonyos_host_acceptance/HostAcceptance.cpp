#include "HostAcceptance.h"

#include <core/event/Event.h>
#include <core/logger/Logger.h>
#include <framework/application/Application.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindow.h>
#include <native_vsync/native_vsync.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

extern "C" OH_NativeVSync *__real_OH_NativeVSync_Create(const char *, unsigned int);
extern "C" int             __real_OH_NativeVSync_RequestFrame(OH_NativeVSync *, OH_NativeVSync_FrameCallback, void *);
extern "C" void            __real_OH_NativeVSync_Destroy(OH_NativeVSync *);
extern "C" int32_t         __real_OH_NativeWindow_NativeObjectReference(void *);
extern "C" int32_t         __real_OH_NativeWindow_NativeObjectUnreference(void *);

namespace host_acceptance {
    namespace {
        using sky::Logger;
        constexpr const char *TAG = "OHOSHostAcceptance";
        struct Pending {
            OH_NativeVSync_FrameCallback callback = nullptr;
            void                        *data     = nullptr;
        };
        struct Probe {
            std::mutex                  mutex;
            std::deque<Pending>         pending;
            Pending                     callbackDuringDestroy;
            std::shared_ptr<const void> heldSurface;
            std::atomic<bool>           failCreate{false}, failRequest{false};
            std::atomic<bool>           holdNextSurface{false};
            std::atomic<uint32_t>       callbacksDuringDestroy{0};
            std::atomic<uint32_t>       createAttempts{0}, requestAttempts{0}, factoryAttempts{0};
            std::atomic<uint32_t>       creates{0}, destroys{0}, requests{0}, references{0}, unreferences{0};
            std::atomic<uint32_t>       applications{0}, applicationDestructions{0}, ticks{0}, changes{0};
            std::atomic<uint32_t>       width{0}, height{0};
            std::atomic<uint64_t>       windowId{0};
            std::atomic<bool>           active{false};
            std::atomic<bool>           liveVsync{false};
            std::mutex                  changedMutex;
            std::condition_variable     changed;
        } probe;
        std::mutex sdkWindowMutex;
        struct Stress {
            std::mutex           mutex;
            bool                 enabled   = false;
            OH_NativeXComponent *component = nullptr;
            uint32_t             created = 0, destroyed = 0, failures = 0, foregrounds = 0, backgrounds = 0;
            uint32_t             initialTicks = 0, initialReferences = 0, initialUnreferences = 0;
            uint32_t             initialApplications = 0, initialDestructions = 0, initialCreates = 0, initialDestroys = 0;
        } stress;

        void Notify()
        {
            std::lock_guard lock(probe.changedMutex);
            probe.changed.notify_all();
        }

        template <typename Predicate>
        bool Wait(Predicate predicate, uint32_t milliseconds = 2000)
        {
            std::unique_lock lock(probe.changedMutex);
            return probe.changed.wait_for(lock, std::chrono::milliseconds(milliseconds), predicate);
        }

        Pending TakeLatest()
        {
            std::lock_guard lock(probe.mutex);
            const auto      result = probe.pending.empty() ? Pending{} : probe.pending.back();
            probe.pending.clear();
            return result;
        }

        void Deliver(const Pending &pending)
        {
            if (pending.callback)
                pending.callback(0, pending.data);
        }

        class TestApplication final : public sky::Application, public sky::IWindowEvent {
        public:
            ~TestApplication() override
            {
                Shutdown();
                events.Reset();
                window.reset();
                ++probe.applicationDestructions;
                Notify();
            }
            void Tick(float) override
            {
                ++probe.ticks;
                Notify();
            }
            void OnWindowResize(const sky::WindowResizeEvent &) override
            {
                RecordWindow();
            }
            void OnSurfaceChanged(const sky::NativeWindow *) override
            {
                RecordWindow();
            }

        protected:
            bool PreInit() override
            {
                window.reset(sky::NativeWindow::Create({}));
                if (!window)
                    return false;
                events.Bind(this, window.get());
                if (probe.holdNextSurface.exchange(false)) {
                    std::lock_guard lock(probe.mutex);
                    probe.heldSurface = window->RetainNativeHandle();
                }
                ++probe.applications;
                RecordWindow();
                return true;
            }

        private:
            void RecordWindow()
            {
                probe.width    = window->GetWidth();
                probe.height   = window->GetHeight();
                probe.windowId = static_cast<uint64_t>(window->GetWinId());
                probe.active   = sky::Platform::Get()->IsFrameActive();
                ++probe.changes;
                Notify();
            }
            std::unique_ptr<sky::NativeWindow>                             window;
            sky::EventBinder<sky::IWindowEvent, const sky::NativeWindow *> events;
        };

        std::unique_ptr<sky::Application> Factory()
        {
            ++probe.factoryAttempts;
            auto application = std::make_unique<TestApplication>();
            application->SetExternalFrameLoop(true);
            return application->Init(0, nullptr) ? std::move(application) : nullptr;
        }

        struct Results {
            uint32_t passed = 0, failed = 0;
            bool     Check(const char *name, bool value)
            {
                value ? ++passed : ++failed;
                if (value)
                    LOG_I(TAG, "PASS %s", name);
                else
                    LOG_E(TAG, "FAIL %s", name);
                return value;
            }
            std::string Json() const
            {
                return "{\"passed\":" + std::to_string(passed) + ",\"failed\":" + std::to_string(failed) +
                       ",\"ticks\":" + std::to_string(probe.ticks.load()) + ",\"requests\":" + std::to_string(probe.requests.load()) + "}";
            }
        };

        bool Attach(sky::OHOSHost &host, const Surface &surface)
        {
            return host.Bind(surface.component) && host.SetSurface(surface.component, surface.window, surface.width, surface.height) &&
                   host.SetActive(true);
        }

        bool ChangeAndWait(sky::OHOSHost &host, const Surface &surface, uint32_t width, uint32_t height)
        {
            const auto changes = probe.changes.load();
            return host.SetSurface(surface.component, surface.window, width, height) && Wait([&] { return probe.changes > changes; });
        }
    } // namespace

    std::string Run(const sky::OHOSApplicationInfo &info, const std::vector<Surface> &surfaces)
    {
        Results result;
        auto   &host = *sky::OHOSHost::Get();
        host.Stop();
        struct Cleanup {
            sky::OHOSHost &host;
            ~Cleanup()
            {
                host.Stop();
                std::lock_guard lock(probe.mutex);
                probe.heldSurface.reset();
                probe.pending.clear();
                probe.callbackDuringDestroy = {};
                probe.failCreate            = false;
                probe.failRequest           = false;
                probe.holdNextSurface       = false;
            }
        } cleanup{host};
        const auto createAttempts = probe.createAttempts.load();
        probe.failCreate          = true;
        result.Check("create failure is reported",
                     !host.Configure(info, Factory) && host.GetStatus() == sky::OHOSHostStatus::FAILED && probe.createAttempts == createAttempts + 1);
        host.Stop();
        probe.failCreate = false;
        result.Check("configure succeeds after failed create and Stop", host.Configure(info, Factory));
        host.Stop();
        if (!result.Check("two real XComponent surfaces supplied", surfaces.size() >= 2 && surfaces[0].window && surfaces[1].window &&
                                                                       surfaces[0].component != surfaces[1].component &&
                                                                       surfaces[0].window != surfaces[1].window))
            return result.Json();
        const auto &first  = surfaces[0];
        const auto &second = surfaces[1];

        const auto destroyed       = probe.applicationDestructions.load();
        const auto requestAttempts = probe.requestAttempts.load();
        probe.failRequest          = true;
        bool started               = host.Configure(info, Factory) && Attach(host, first);
        result.Check("request failure terminates session", started && Wait([&] { return host.GetStatus() == sky::OHOSHostStatus::FAILED; }) &&
                                                               probe.requestAttempts == requestAttempts + 1);
        host.Stop();
        result.Check("request failure destroys application", probe.applicationDestructions == destroyed + 1);
        probe.failRequest = false;

        const auto factoryAttempts = probe.factoryAttempts.load();
        started                    = host.Configure(info, [] {
            ++probe.factoryAttempts;
            return std::unique_ptr<sky::Application>{};
        }) && Attach(host, first);
        result.Check("factory failure terminates session", started && Wait([&] { return host.GetStatus() == sky::OHOSHostStatus::FAILED; }) &&
                                                               probe.factoryAttempts == factoryAttempts + 1);
        host.Stop();

        const auto requests   = probe.requests.load();
        const auto references = probe.references.load();
        probe.holdNextSurface = true;
        if (!result.Check("normal session starts",
                          host.Configure(info, Factory) && Attach(host, first) && Wait([&] { return probe.requests > requests; }))) {
            host.Stop();
            return result.Json();
        }
        result.Check("window shares lease without second SDK retain", probe.references == references + 1);
        result.Check("foreign XComponent rejected",
                     !host.Bind(second.component) && !host.SetSurface(second.component, second.window, second.width, second.height));
        const auto identity = probe.windowId.load();
        const auto initial  = TakeLatest();
        const auto ticks    = probe.ticks.load();
        Deliver(initial);
        result.Check("current VSync ticks once", Wait([&] { return probe.ticks == ticks + 1 && probe.requests > requests + 1; }));

        auto late = TakeLatest();
        result.Check("zero extent is applied", ChangeAndWait(host, first, 0, 0) && !probe.active && probe.width == 0 && probe.height == 0);
        const auto zeroTicks = probe.ticks.load();
        Deliver(late);
        result.Check("zero extent ignores old callback", !Wait([&] { return probe.ticks != zeroTicks; }, 50));
        result.Check("nonzero extent restores same window",
                     ChangeAndWait(host, first, first.width, first.height) && probe.windowId == identity && probe.active);
        result.Check("background event applied before resize acknowledgement",
                     host.SetActive(false) && ChangeAndWait(host, first, first.width + 1, first.height) && !probe.active);
        late                       = TakeLatest();
        const auto backgroundTicks = probe.ticks.load();
        Deliver(late);
        result.Check("background ignores queued VSync", !Wait([&] { return probe.ticks != backgroundTicks; }, 50));
        const auto foregroundRequests = probe.requests.load();
        result.Check("foreground restores activity", host.SetActive(true) && ChangeAndWait(host, first, first.width, first.height) && probe.active &&
                                                         Wait([&] { return probe.requests > foregroundRequests; }));

        const auto beforeReplacement = probe.changes.load();
        result.Check("old component is unbound", host.SetSurface(first.component, nullptr, 0, 0));
        result.Check("replacement binds with real SDK surface",
                     host.Bind(second.component) && host.SetSurface(second.component, second.window, second.width, second.height) &&
                         Wait([&] { return probe.changes > beforeReplacement && probe.width == second.width && probe.height == second.height; }));
        result.Check("replacement preserves WindowID", probe.windowId == identity);
        result.Check("late old surface event rejected", !host.SetSurface(first.component, first.window, first.width, first.height));

        late = TakeLatest();
        {
            std::lock_guard lock(probe.mutex);
            probe.callbackDuringDestroy = late;
        }
        const auto callbacksDuringDestroy = probe.callbacksDuringDestroy.load();
        const auto closingTicks           = probe.ticks.load();
        host.Stop();
        result.Check("callback delivered during VSync destroy cannot tick",
                     probe.callbacksDuringDestroy == callbacksDuringDestroy + 1 && probe.ticks == closingTicks);
        const auto stoppedTicks = probe.ticks.load();
        Deliver(late);
        result.Check("Stop joins before returning", host.GetStatus() == sky::OHOSHostStatus::STOPPED && probe.ticks == stoppedTicks);
        const auto nextRequests = probe.requests.load();
        started                 = host.Configure(info, Factory) && Attach(host, second) && Wait([&] { return probe.requests > nextRequests; });
        result.Check("configure after Stop creates new session", started);
        const auto fresh = TakeLatest();
        result.Check("new session has distinct callback generation", late.callback && fresh.callback && late.data != fresh.data);
        const auto newTicks = probe.ticks.load();
        Deliver(late);
        result.Check("late callback cannot tick new session", !Wait([&] { return probe.ticks != newTicks; }, 50));
        Deliver(fresh);
        result.Check("new callback still advances new session", Wait([&] { return probe.ticks == newTicks + 1; }));
        host.Stop();
        result.Check("retired surface lease outlives Stop and replacement", probe.references == probe.unreferences + 1);
        {
            std::lock_guard lock(probe.mutex);
            probe.heldSurface.reset();
        }
        result.Check("all SDK-retained windows released", probe.references == probe.unreferences);
        result.Check("all created applications destroyed", probe.applications == probe.applicationDestructions);
        result.Check("all real VSync handles destroyed", probe.creates == probe.destroys);
        return result.Json();
    }

    bool RetainFixtureSurface(OHNativeWindow *window)
    {
        std::lock_guard lock(sdkWindowMutex);
        return __real_OH_NativeWindow_NativeObjectReference(window) == 0;
    }

    bool ReleaseFixtureSurface(OHNativeWindow *window)
    {
        std::lock_guard lock(sdkWindowMutex);
        return __real_OH_NativeWindow_NativeObjectUnreference(window) == 0;
    }

    bool StartStress(const sky::OHOSApplicationInfo &info)
    {
        std::lock_guard lock(stress.mutex);
        auto           &host = *sky::OHOSHost::Get();
        host.Stop();
        stress.component = nullptr;
        stress.created = stress.destroyed = stress.failures = stress.foregrounds = stress.backgrounds = 0;
        stress.initialTicks                                                                           = probe.ticks;
        stress.initialReferences                                                                      = probe.references;
        stress.initialUnreferences                                                                    = probe.unreferences;
        stress.initialApplications                                                                    = probe.applications;
        stress.initialDestructions                                                                    = probe.applicationDestructions;
        stress.initialCreates                                                                         = probe.creates;
        stress.initialDestroys                                                                        = probe.destroys;
        probe.liveVsync                                                                               = true;
        stress.enabled = host.Configure(info, Factory) && host.SetActive(true);
        if (!stress.enabled) {
            host.Stop();
            probe.liveVsync = false;
        }
        return stress.enabled;
    }

    void StressSurfaceChanged(const Surface &surface)
    {
        std::lock_guard lock(stress.mutex);
        if (!stress.enabled)
            return;
        auto &host = *sky::OHOSHost::Get();
        if (stress.component == nullptr) {
            if (!host.Bind(surface.component)) {
                ++stress.failures;
                return;
            }
            stress.component = surface.component;
            ++stress.created;
        }
        if (stress.component == surface.component && !host.SetSurface(surface.component, surface.window, surface.width, surface.height))
            ++stress.failures;
    }

    void StressSurfaceDestroyed(OH_NativeXComponent *component)
    {
        std::lock_guard lock(stress.mutex);
        if (!stress.enabled || stress.component != component)
            return;
        if (!sky::OHOSHost::Get()->SetSurface(component, nullptr, 0, 0))
            ++stress.failures;
        stress.component = nullptr;
        ++stress.destroyed;
    }

    void StressActive(bool active)
    {
        std::lock_guard lock(stress.mutex);
        if (!stress.enabled)
            return;
        active ? ++stress.foregrounds : ++stress.backgrounds;
        if (!sky::OHOSHost::Get()->SetActive(active))
            ++stress.failures;
    }

    std::string StressStats()
    {
        std::lock_guard lock(stress.mutex);
        return "{\"stress\":true,\"created\":" + std::to_string(stress.created) + ",\"destroyed\":" + std::to_string(stress.destroyed) +
               ",\"failures\":" + std::to_string(stress.failures) + ",\"foregrounds\":" + std::to_string(stress.foregrounds) +
               ",\"backgrounds\":" + std::to_string(stress.backgrounds) + ",\"ticks\":" + std::to_string(probe.ticks - stress.initialTicks) +
               ",\"retained\":" + std::to_string(probe.references - stress.initialReferences) +
               ",\"released\":" + std::to_string(probe.unreferences - stress.initialUnreferences) +
               ",\"applications\":" + std::to_string(probe.applications - stress.initialApplications) +
               ",\"applicationDestructions\":" + std::to_string(probe.applicationDestructions - stress.initialDestructions) +
               ",\"vsyncCreated\":" + std::to_string(probe.creates - stress.initialCreates) +
               ",\"vsyncDestroyed\":" + std::to_string(probe.destroys - stress.initialDestroys) +
               ",\"status\":" + std::to_string(static_cast<int>(sky::OHOSHost::Get()->GetStatus())) + "}";
    }

    std::string StopStress()
    {
        {
            std::lock_guard lock(stress.mutex);
            sky::OHOSHost::Get()->Stop();
            stress.enabled   = false;
            stress.component = nullptr;
            probe.liveVsync  = false;
        }
        const auto report = StressStats();
        LOG_I(TAG, "STRESS %s", report.c_str());
        return report;
    }
} // namespace host_acceptance

extern "C" OH_NativeVSync *__wrap_OH_NativeVSync_Create(const char *name, unsigned int length)
{
    using namespace host_acceptance;
    ++probe.createAttempts;
    if (probe.failCreate)
        return nullptr;
    auto *result = __real_OH_NativeVSync_Create(name, length);
    if (result)
        ++probe.creates;
    return result;
}

extern "C" int __wrap_OH_NativeVSync_RequestFrame(OH_NativeVSync *vsync, OH_NativeVSync_FrameCallback callback, void *data)
{
    using namespace host_acceptance;
    ++probe.requestAttempts;
    if (probe.failRequest)
        return 50401000;
    if (probe.liveVsync) {
        const auto result = __real_OH_NativeVSync_RequestFrame(vsync, callback, data);
        if (result == 0)
            ++probe.requests;
        Notify();
        return result;
    }
    {
        std::lock_guard lock(probe.mutex);
        probe.pending.push_back({callback, data});
        ++probe.requests;
    }
    Notify();
    return 0;
}

extern "C" void __wrap_OH_NativeVSync_Destroy(OH_NativeVSync *vsync)
{
    host_acceptance::Pending pending;
    {
        std::lock_guard lock(host_acceptance::probe.mutex);
        pending = std::exchange(host_acceptance::probe.callbackDuringDestroy, {});
    }
    if (pending.callback) {
        host_acceptance::Deliver(pending);
        ++host_acceptance::probe.callbacksDuringDestroy;
    }
    __real_OH_NativeVSync_Destroy(vsync);
    ++host_acceptance::probe.destroys;
    host_acceptance::Notify();
}

extern "C" int32_t __wrap_OH_NativeWindow_NativeObjectReference(void *window)
{
    std::lock_guard lock(host_acceptance::sdkWindowMutex);
    const auto      result = __real_OH_NativeWindow_NativeObjectReference(window);
    if (result == 0)
        ++host_acceptance::probe.references;
    return result;
}

extern "C" int32_t __wrap_OH_NativeWindow_NativeObjectUnreference(void *window)
{
    std::lock_guard lock(host_acceptance::sdkWindowMutex);
    const auto      result = __real_OH_NativeWindow_NativeObjectUnreference(window);
    if (result == 0)
        ++host_acceptance::probe.unreferences;
    return result;
}
