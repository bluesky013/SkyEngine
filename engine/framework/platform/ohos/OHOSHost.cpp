#include <framework/platform/OHOSHost.h>

#include "OHOSDialog.h"
#include "OHOSIME.h"
#include "OHOSNativeInput.h"
#include "OHOSPlatform.h"
#include "OHOSWindow.h"

#include <core/logger/Logger.h>
#include <framework/application/Application.h>
#include <native_vsync/native_vsync.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace sky {
    namespace {
        constexpr const char *TAG = "OHOSHost";

        struct HostEvent {
            enum class Type { SURFACE, ACTIVE, FOCUS, TOUCH, MOUSE, WHEEL, KEY, TEXT, TEXT_KEY, DIALOG };
            Type                                 type = Type::SURFACE;
            std::shared_ptr<const void>          surface;
            uint32_t                             width        = 0;
            uint32_t                             height       = 0;
            bool                                 active       = false;
            OHNativeWindow                      *nativeWindow = nullptr;
            OHOSTouchAction                      touchAction  = OHOSTouchAction::MOVE;
            OH_NativeXComponent_MouseEventAction mouseAction  = OH_NATIVEXCOMPONENT_MOUSE_MOVE;
            int32_t                              contact      = 0;
            int32_t                              x            = 0;
            int32_t                              y            = 0;
            uint32_t                             buttons      = 0;
            float                                scroll       = 0.f;
            ScanCode                             key          = ScanCode::KEY_NUM;
            KeyModFlags                          modifiers;
            bool                                 down      = false;
            uint32_t                             count     = 0;
            uint64_t                             textToken = 0;
            std::u16string                       text;
        };
    } // namespace

    struct OHOSHost::Impl {
        bool Configure(OHOSApplicationInfo info, OHOSApplicationFactory value)
        {
            std::lock_guard  sessionLock(sessionMutex);
            std::unique_lock lock(mutex);
            if (attempted || stopping || !value) {
                return false;
            }
            factory   = std::move(value);
            status    = OHOSHostStatus::NEW;
            attempted = true;
            worker    = std::thread([this, info = std::move(info)]() mutable { Run(std::move(info)); });
            changed.wait(lock, [this] { return initialized; });
            return status != OHOSHostStatus::FAILED;
        }

        bool Bind(OH_NativeXComponent *value)
        {
            std::lock_guard lock(mutex);
            if (value == nullptr || stopping || (component != nullptr && component != value)) {
                return false;
            }
            component = value;
            return true;
        }

        bool SetSurface(OH_NativeXComponent *source, OHNativeWindow *value, uint32_t width, uint32_t height)
        {
            std::lock_guard lock(mutex);
            if (source == nullptr || stopping || component != source) {
                return false;
            }
            HostEvent event;
            if (value != nullptr) {
                {
                    std::lock_guard windowLock(*windowMutex);
                    const int       result = OH_NativeWindow_NativeObjectReference(value);
                    if (result != 0) {
                        LOG_E(TAG, "Native window reference failed: %d", result);
                        return false;
                    }
                }
                // The SDK calls are not thread-safe; the shared lock also outlives retired GPU leases.
                event.surface = {value, [guard = windowMutex](const void *window) {
                                     std::lock_guard windowLock(*guard);
                                     const int       result = OH_NativeWindow_NativeObjectUnreference(const_cast<void *>(window));
                                     if (result != 0) {
                                         LOG_E(TAG, "Native window unreference failed: %d", result);
                                     }
                                 }};
            }
            event.width  = width;
            event.height = height;
            events.push_back(std::move(event));
            if (value == nullptr) {
                component = nullptr;
            }
            changed.notify_one();
            return true;
        }

        bool SetActive(bool value)
        {
            std::lock_guard lock(mutex);
            if (stopping) {
                return false;
            }
            HostEvent event;
            event.type   = HostEvent::Type::ACTIVE;
            event.active = value;
            events.push_back(std::move(event));
            changed.notify_one();
            return true;
        }

        bool PostInput(OH_NativeXComponent *source, HostEvent event)
        {
            std::lock_guard lock(mutex);
            if (source == nullptr || source != component || !attempted || stopping) {
                return false;
            }
            events.push_back(std::move(event));
            changed.notify_one();
            return true;
        }

        bool Owns(OH_NativeXComponent *source)
        {
            std::lock_guard lock(mutex);
            return source != nullptr && source == component && attempted && !stopping;
        }

        void PostText(uint64_t token, HostEvent event)
        {
            std::lock_guard lock(mutex);
            if (!attempted || stopping || token != textGeneration) {
                return;
            }
            event.textToken = token;
            events.push_back(std::move(event));
            changed.notify_one();
        }

        bool StartTextInput(uint32_t windowId)
        {
            uint64_t token = 0;
            {
                std::lock_guard lock(mutex);
                if (!attempted || stopping) {
                    return false;
                }
                token = ++textGeneration;
            }
            const bool started = OHOSIME::Get()->Start(
                windowId,
                [token](std::u16string text) {
                    HostEvent event;
                    event.type = HostEvent::Type::TEXT;
                    event.text = std::move(text);
                    OHOSHost::Get()->impl->PostText(token, std::move(event));
                },
                [token](ScanCode code, uint32_t count) {
                    HostEvent event;
                    event.type  = HostEvent::Type::TEXT_KEY;
                    event.key   = code;
                    event.count = count;
                    OHOSHost::Get()->impl->PostText(token, std::move(event));
                });
            if (!started) {
                std::lock_guard lock(mutex);
                ++textGeneration;
            }
            return started;
        }

        bool StopTextInput()
        {
            {
                std::lock_guard lock(mutex);
                ++textGeneration;
            }
            return OHOSIME::Get()->Stop();
        }

        bool Deliver(OHOSPlatform &platform, const HostEvent &event)
        {
            switch (event.type) {
            case HostEvent::Type::DIALOG: PollOHOSTextInputDialog(); return true;
            case HostEvent::Type::SURFACE: {
                const auto *window = platform.GetInputWindow();
                if (event.surface.get() != platform.GetMainWinHandle() || event.width == 0 || event.height == 0 ||
                    (window != nullptr && (window->GetWidth() != event.width || window->GetHeight() != event.height))) {
                    mouseButtons = 0;
                }
                return platform.SetSurface(event.surface, event.width, event.height, 1.0f);
            }
            case HostEvent::Type::ACTIVE:
                if (!event.active)
                    mouseButtons = 0;
                return platform.SetActive(event.active);
            case HostEvent::Type::FOCUS:
                if (!event.active)
                    mouseButtons = 0;
                return platform.SetFocus(event.active);
            default: break;
            }
            auto *window = platform.GetInputWindow();
            if (window == nullptr || (event.nativeWindow != nullptr && event.nativeWindow != platform.GetMainWinHandle())) {
                return true;
            }
            if (event.type == HostEvent::Type::TEXT || event.type == HostEvent::Type::TEXT_KEY) {
                std::lock_guard lock(mutex);
                if (event.textToken != textGeneration || stopping) {
                    return true;
                }
            }
            switch (event.type) {
            case HostEvent::Type::TOUCH: window->Touch(event.touchAction, event.contact, event.x, event.y); break;
            case HostEvent::Type::MOUSE:
                switch (event.mouseAction) {
                case OH_NATIVEXCOMPONENT_MOUSE_PRESS: mouseButtons |= event.buttons; break;
                case OH_NATIVEXCOMPONENT_MOUSE_RELEASE: mouseButtons &= ~event.buttons; break;
                case OH_NATIVEXCOMPONENT_MOUSE_CANCEL: mouseButtons = 0; break;
                default: break;
                }
                window->Mouse(event.x, event.y, mouseButtons, 0.f);
                break;
            case HostEvent::Type::WHEEL: window->Mouse(event.x, event.y, mouseButtons, event.scroll); break;
            case HostEvent::Type::KEY: window->Key(event.key, event.down, event.modifiers); break;
            case HostEvent::Type::TEXT: window->Text(event.text); break;
            case HostEvent::Type::TEXT_KEY: window->TextKey(event.key, event.count); break;
            default: break;
            }
            return true;
        }

        void Stop()
        {
            std::lock_guard sessionLock(sessionMutex);
            {
                std::lock_guard lock(mutex);
                stopping = true;
                ++generation;
                ++textGeneration;
                changed.notify_all();
            }
            if (worker.joinable()) {
                worker.join();
            }
            std::lock_guard lock(mutex);
            events.clear();
            factory      = {};
            component    = nullptr;
            attempted    = false;
            initialized  = false;
            framePending = false;
            mouseButtons = 0;
            stopping     = false;
            if (status != OHOSHostStatus::FAILED) {
                status = OHOSHostStatus::STOPPED;
            }
        }

        void SignalFrame(uintptr_t token)
        {
            std::lock_guard lock(mutex);
            if (attempted && !stopping && token == generation) {
                framePending = true;
                changed.notify_one();
            }
        }

        static void OnVSync(long long, void *token)
        {
            OHOSHost::Get()->impl->SignalFrame(reinterpret_cast<uintptr_t>(token));
        }

        void Run(OHOSApplicationInfo info)
        {
            const bool      platformReady = Platform::Get()->Init({&info});
            auto           *platform      = platformReady ? static_cast<OHOSPlatform *>(Platform::Get()->GetImpl()) : nullptr;
            OH_NativeVSync *vsync         = platformReady ? OH_NativeVSync_Create(TAG, sizeof("OHOSHost") - 1) : nullptr;
            {
                std::lock_guard lock(mutex);
                status      = vsync != nullptr ? OHOSHostStatus::WAITING_FOR_SURFACE : OHOSHostStatus::FAILED;
                initialized = true;
                changed.notify_all();
            }
            std::unique_ptr<Application> application;
            bool                         frameRequested = false;
            bool                         failed         = vsync == nullptr;
            while (!failed) {
                std::deque<HostEvent> batch;
                bool                  tick = false;
                {
                    std::unique_lock lock(mutex);
                    changed.wait(lock, [this] { return stopping || framePending || !events.empty(); });
                    if (stopping) {
                        break;
                    }
                    batch.swap(events);
                    tick                 = framePending;
                    framePending         = false;
                    const bool lifecycle = std::any_of(batch.begin(), batch.end(), [](const HostEvent &event) {
                        return event.type == HostEvent::Type::SURFACE || event.type == HostEvent::Type::ACTIVE;
                    });
                    if (lifecycle) {
                        ++generation;
                        tick           = false;
                        frameRequested = false;
                    }
                }
                if (tick) {
                    frameRequested = false;
                }
                for (const auto &event : batch) {
                    if (!Deliver(*platform, event)) {
                        failed = true;
                        break;
                    }
                }
                if (failed) {
                    break;
                }
                if (!application && platform->IsFrameActive()) {
                    application = factory();
                    if (!application) {
                        failed = true;
                        break;
                    }
                    status = OHOSHostStatus::RUNNING;
                }
                if (application && tick && platform->IsFrameActive()) {
                    application->Loop();
                    if (application->IsExit()) {
                        break;
                    }
                }
                if (application && platform->IsFrameActive() && !frameRequested) {
                    uintptr_t token = 0;
                    {
                        std::lock_guard lock(mutex);
                        token = generation;
                    }
                    const int result = OH_NativeVSync_RequestFrame(vsync, OnVSync, reinterpret_cast<void *>(token));
                    if (result != 0) {
                        LOG_E(TAG, "NativeVSync request failed: %d", result);
                        failed = true;
                        break;
                    }
                    frameRequested = true;
                }
            }
            {
                std::lock_guard lock(mutex);
                stopping = true;
                ++generation;
                events.clear();
            }
            // Only this thread uses the VSync handle; the process receiver rejects late callback generations.
            if (vsync != nullptr) {
                OH_NativeVSync_Destroy(vsync);
            }
            application.reset();
            if (Platform::Get()->GetImpl() != nullptr) {
                Platform::Get()->Shutdown();
            }
            status = failed ? OHOSHostStatus::FAILED : OHOSHostStatus::STOPPED;
            if (failed) {
                LOG_E(TAG, "Native host initialization or frame loop failed");
            }
        }

        std::mutex                  sessionMutex;
        std::mutex                  mutex;
        std::condition_variable     changed;
        std::deque<HostEvent>       events;
        std::thread                 worker;
        std::shared_ptr<std::mutex> windowMutex = std::make_shared<std::mutex>();
        OHOSApplicationFactory      factory;
        OH_NativeXComponent        *component = nullptr;
        std::atomic<OHOSHostStatus> status{OHOSHostStatus::NEW};
        uintptr_t                   generation     = 1;
        uint64_t                    textGeneration = 0;
        uint32_t                    mouseButtons   = 0;
        bool                        attempted      = false;
        bool                        initialized    = false;
        bool                        stopping       = false;
        bool                        framePending   = false;
    };

    OHOSHost::OHOSHost() : impl(std::make_unique<Impl>())
    {
    }

    OHOSHost::~OHOSHost()
    {
        Stop();
    }

    bool OHOSHost::Configure(OHOSApplicationInfo info, OHOSApplicationFactory factory)
    {
        return impl->Configure(std::move(info), std::move(factory));
    }

    bool OHOSHost::Bind(OH_NativeXComponent *component)
    {
        return impl->Bind(component);
    }

    bool OHOSHost::SetSurface(OH_NativeXComponent *source, OHNativeWindow *surface, uint32_t width, uint32_t height)
    {
        return impl->SetSurface(source, surface, width, height);
    }

    bool OHOSHost::SetActive(bool active)
    {
        return impl->SetActive(active);
    }

    bool OHOSHost::SetFocus(OH_NativeXComponent *source, bool focused)
    {
        HostEvent event;
        event.type   = HostEvent::Type::FOCUS;
        event.active = focused;
        return impl->PostInput(source, std::move(event));
    }

    bool OHOSHost::DispatchTouch(OH_NativeXComponent *source, OHNativeWindow *surface)
    {
        if (!impl->Owns(source))
            return false;
        OH_NativeXComponent_TouchEvent touch{};
        if (OH_NativeXComponent_GetTouchEvent(source, surface, &touch) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
            touch.numPoints > OH_NATIVE_XCOMPONENT_MAX_TOUCH_POINTS_NUMBER) {
            LOG_E(TAG, "Failed to obtain a valid native touch event");
            return false;
        }
        HostEvent event;
        event.type         = HostEvent::Type::TOUCH;
        event.nativeWindow = surface;
        event.contact      = touch.id;
        switch (touch.type) {
        case OH_NATIVEXCOMPONENT_DOWN: event.touchAction = OHOSTouchAction::DOWN; break;
        case OH_NATIVEXCOMPONENT_MOVE: event.touchAction = OHOSTouchAction::MOVE; break;
        case OH_NATIVEXCOMPONENT_UP: event.touchAction = OHOSTouchAction::UP; break;
        case OH_NATIVEXCOMPONENT_CANCEL: event.touchAction = OHOSTouchAction::CANCEL; break;
        default: return false;
        }
        if (event.touchAction == OHOSTouchAction::MOVE && touch.numPoints != 0) {
            bool accepted = true;
            for (uint32_t index = 0; index < touch.numPoints; ++index) {
                const auto &point = touch.touchPoints[index];
                event.contact     = point.id;
                if (!OHOSPixelCoordinate(point.x, event.x) || !OHOSPixelCoordinate(point.y, event.y)) {
                    return false;
                }
                accepted = impl->PostInput(source, event) && accepted;
            }
            return accepted;
        }
        if (event.touchAction != OHOSTouchAction::CANCEL && (!OHOSPixelCoordinate(touch.x, event.x) || !OHOSPixelCoordinate(touch.y, event.y))) {
            return false;
        }
        return impl->PostInput(source, std::move(event));
    }

    bool OHOSHost::DispatchMouse(OH_NativeXComponent *source, OHNativeWindow *surface)
    {
        if (!impl->Owns(source))
            return false;
        OH_NativeXComponent_MouseEvent mouse{};
        if (OH_NativeXComponent_GetMouseEvent(source, surface, &mouse) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
            LOG_E(TAG, "Failed to obtain a native mouse event");
            return false;
        }
        HostEvent event;
        event.type         = HostEvent::Type::MOUSE;
        event.nativeWindow = surface;
        event.mouseAction  = mouse.action;
        event.buttons      = static_cast<uint32_t>(mouse.button) & 7U;
        if (!OHOSPixelCoordinate(mouse.x, event.x) || !OHOSPixelCoordinate(mouse.y, event.y))
            return false;
        switch (mouse.action) {
        case OH_NATIVEXCOMPONENT_MOUSE_PRESS:
        case OH_NATIVEXCOMPONENT_MOUSE_RELEASE:
        case OH_NATIVEXCOMPONENT_MOUSE_MOVE:
        case OH_NATIVEXCOMPONENT_MOUSE_CANCEL: return impl->PostInput(source, std::move(event));
        default: return false;
        }
    }

    bool OHOSHost::DispatchKey(OH_NativeXComponent *source, OHNativeWindow *surface)
    {
        if (!impl->Owns(source))
            return false;
        OH_NativeXComponent_KeyEvent *key       = nullptr;
        OH_NativeXComponent_KeyAction action    = OH_NATIVEXCOMPONENT_KEY_ACTION_UNKNOWN;
        OH_NativeXComponent_KeyCode   code      = ::KEY_UNKNOWN;
        uint64_t                      modifiers = 0;
        bool                          num       = false;
        bool                          caps      = false;
        if (OH_NativeXComponent_GetKeyEvent(source, &key) != 0 || key == nullptr || OH_NativeXComponent_GetKeyEventAction(key, &action) != 0 ||
            OH_NativeXComponent_GetKeyEventCode(key, &code) != 0 || OH_NativeXComponent_GetKeyEventModifierKeyStates(key, &modifiers) != 0 ||
            OH_NativeXComponent_GetKeyEventNumLockState(key, &num) != 0 || OH_NativeXComponent_GetKeyEventCapsLockState(key, &caps) != 0) {
            LOG_E(TAG, "Failed to obtain a native key event");
            return false;
        }
        HostEvent event;
        event.type         = HostEvent::Type::KEY;
        event.nativeWindow = surface;
        event.key          = OHOSKeyCodeToScanCode(code);
        event.modifiers    = OHOSKeyModifiers(modifiers, num, caps);
        if (event.key == ScanCode::KEY_NUM || (action != OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN && action != OH_NATIVEXCOMPONENT_KEY_ACTION_UP))
            return false;
        event.down = action == OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN;
        return impl->PostInput(source, std::move(event));
    }

    bool OHOSHost::DispatchAxis(OH_NativeXComponent *source, ArkUI_UIInputEvent *axis)
    {
        if (!impl->Owns(source) || axis == nullptr || OH_ArkUI_UIInputEvent_GetType(axis) != ARKUI_UIINPUTEVENT_TYPE_AXIS)
            return false;
        HostEvent event;
        event.type         = HostEvent::Type::WHEEL;
        const double delta = OH_ArkUI_AxisEvent_GetVerticalAxisValue(axis);
        // Mouse deltas are degrees; touchpad deltas are pixels. Preserve fractional wheel steps.
        const double step   = OH_ArkUI_UIInputEvent_GetToolType(axis) == UI_INPUT_EVENT_TOOL_TYPE_TOUCHPAD ? 40.0 : 15.0;
        const double scroll = -delta / step;
        if (!std::isfinite(scroll) || std::abs(scroll) > std::numeric_limits<float>::max() ||
            !OHOSPixelCoordinate(OH_ArkUI_PointerEvent_GetX(axis), event.x) || !OHOSPixelCoordinate(OH_ArkUI_PointerEvent_GetY(axis), event.y))
            return false;
        event.scroll = static_cast<float>(scroll);
        return impl->PostInput(source, std::move(event));
    }

    bool OHOSHost::StartTextInput(uint32_t windowId)
    {
        return impl->StartTextInput(windowId);
    }

    bool OHOSHost::StopTextInput()
    {
        return impl->StopTextInput();
    }

    bool OHOSHost::SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        return OHOSIME::Get()->SetRect(x, y, width, height);
    }

    void OHOSHost::NotifyTextInputDialog()
    {
        std::lock_guard lock(impl->mutex);
        if (impl->attempted && !impl->stopping) {
            HostEvent event;
            event.type = HostEvent::Type::DIALOG;
            impl->events.push_back(std::move(event));
            impl->changed.notify_one();
        }
    }

    void OHOSHost::Stop()
    {
        impl->Stop();
    }

    OHOSHostStatus OHOSHost::GetStatus() const
    {
        return impl->status.load();
    }
} // namespace sky
