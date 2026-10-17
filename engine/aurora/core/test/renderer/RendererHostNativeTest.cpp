//
// Created on 2026/10/17.
//

#include "HostSceneFixture.h"
#include <array>
#include <aurora/adaptor/AuroraModule.h>
#include <aurora/rhi/Instance.h>
#include <chrono>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindow.h>
#include <gtest/gtest.h>
#include <thread>

#include <D3D12Device.h>
#include <d3d12sdklayers.h>
#include <windows.h>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    class RendererHostNativeTest : public testing::TestWithParam<uint32_t>, public ISystemNotify {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(Platform::Get()->Init({}));
            window.reset(NativeWindow::Create({128, 96, "AuroraB14Host", "Aurora host test", nullptr}));
            ASSERT_TRUE(window);
            Interface<ISystemNotify>::Get()->Register(*this);
            args.args = {"RendererHostNativeTest", "-r", GetParam() == 2 ? "dx12" : "vulkan"};
            if (GetParam() == 1)
                args.args.push_back("--aurora-vulkan-render-pass");
            ASSERT_TRUE(module.Init(args));
            renderer = Interface<IRenderer>::Get()->GetApi();
            ASSERT_TRUE(renderer);
            if (GetParam() == 2) {
                ASSERT_TRUE(
                    SUCCEEDED(static_cast<D3D12Device *>(Instance::Get()->GetDevice())->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
        }
        void TearDown() override
        {
            module.Shutdown();
            EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
            EXPECT_EQ(Interface<IRendererProvider>::Get()->GetApi(), nullptr);
            CheckDebug();
            Interface<ISystemNotify>::Get()->UnRegister();
            window.reset();
            Platform::Get()->Shutdown();
            Instance::Destroy();
        }
        void CheckDebug()
        {
            if (debug) {
                for (UINT64 i = 0; i < debug->GetNumStoredMessages(); ++i) {
                    SIZE_T size = 0;
                    debug->GetMessage(i, nullptr, &size);
                    std::vector<char> bytes(size);
                    auto             *message = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
                    if (SUCCEEDED(debug->GetMessage(i, message, &size)))
                        EXPECT_GT(message->Severity, D3D12_MESSAGE_SEVERITY_ERROR) << message->pDescription;
                }
                debug.Reset();
            }
        }
        void SetExit() override
        {
            failed = true;
        }
        void *GetMainWindowHandle() const override
        {
            return window->GetNativeHandle();
        }
        template <typename Predicate>
        bool Until(Predicate predicate)
        {
            const auto deadline = std::chrono::steady_clock::now() + 10s;
            do {
                bool quit = false;
                Platform::Get()->PoolEvent(quit);
                std::array<RendererEvent, 16> batch;
                const auto                    count = renderer->PollEvents(batch);
                for (auto &event : std::span(batch).first(count)) {
                    if (const auto *error = std::get_if<RendererFailure>(&event))
                        ADD_FAILURE() << error->error.message;
                    events.push_back(std::move(event));
                }
                if (predicate())
                    return true;
                module.Tick(1.f / 60);
                std::this_thread::sleep_for(1ms);
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        }
        bool Completed(bool gpu = true) const
        {
            return std::any_of(events.begin(), events.end(), [&](const auto &event) {
                const auto *frame = std::get_if<FrameCompleted>(&event);
                return frame && (!gpu || frame->submissionSerial != 0);
            });
        }
        std::unique_ptr<NativeWindow>           window;
        AuroraModule                            module;
        StartArguments                          args;
        IRenderer                              *renderer = nullptr;
        std::vector<RendererEvent>              events;
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> debug;
        bool                                    failed = false;
    };
} // namespace

TEST_P(RendererHostNativeTest, B14LauncherResizesMinimizesAndDefersNativeCloseUntilRetirement)
{
    module.Start();
    ASSERT_TRUE(window->IsCloseDeferred());
    module.Tick(1.f / 60);
    ASSERT_TRUE(Until([&] { return Completed(); }));
    events.clear();
    const auto handle = static_cast<HWND>(window->GetNativeHandle());
    SetWindowPos(handle, nullptr, 0, 0, 192, 144, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    module.Tick(1.f / 60);
    ASSERT_TRUE(Until([&] { return Completed(); }));
    events.clear();
    ShowWindow(handle, SW_MINIMIZE);
    module.Tick(1.f / 60);
    ASSERT_TRUE(Until([&] { return Completed(false); }));
    events.clear();
    ShowWindow(handle, SW_SHOWNOACTIVATE);
    module.Tick(1.f / 60);
    ASSERT_TRUE(Until([&] { return Completed(); }));
    events.clear();
    SendMessageW(handle, WM_CLOSE, 0, 0);
    EXPECT_TRUE(IsWindow(handle));
    module.Tick(1.f / 60);
    ASSERT_TRUE(Until([&] {
        return std::any_of(events.begin(), events.end(), [](const auto &event) { return std::holds_alternative<ViewportDestroyed>(event); });
    }));
    EXPECT_TRUE(IsWindow(handle));
    EXPECT_FALSE(failed);
    module.Shutdown();
    EXPECT_FALSE(window->IsCloseDeferred());
    EXPECT_TRUE(IsWindow(handle));
    window.reset();
    EXPECT_FALSE(IsWindow(handle));
}

TEST_P(RendererHostNativeTest, B14OnePublishedRendererAndReloadGetsANewDeviceEpoch)
{
    const auto   epoch = renderer->GetInfo().deviceEpoch;
    AuroraModule duplicate;
    EXPECT_FALSE(duplicate.Init(args));
    duplicate.Shutdown();
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), renderer);
    auto extra = Interface<IRendererProvider>::Get()->GetApi()->Create({});
    ASSERT_TRUE(std::holds_alternative<RendererError>(extra));
    EXPECT_EQ(std::get<RendererError>(extra).code, RendererErrorCode::NOT_READY);
    module.Start();
    module.Tick(1.f / 60);
    module.Shutdown();
    CheckDebug();
    EXPECT_TRUE(IsWindow(static_cast<HWND>(window->GetNativeHandle())));
    ASSERT_TRUE(module.Init(args));
    renderer = Interface<IRenderer>::Get()->GetApi();
    ASSERT_TRUE(renderer);
    if (GetParam() == 2) {
        ASSERT_TRUE(SUCCEEDED(static_cast<D3D12Device *>(Instance::Get()->GetDevice())->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
        debug->ClearStoredMessages();
    }
    EXPECT_GT(renderer->GetInfo().deviceEpoch, epoch);
    module.Start();
    module.Tick(1.f / 60);
    ASSERT_TRUE(Until([&] { return Completed(); }));
    EXPECT_FALSE(failed);
}

INSTANTIATE_TEST_SUITE_P(Windows, RendererHostNativeTest, testing::Values(0u, 1u, 2u));

TEST_P(RendererHostNativeTest, B14NonemptySceneUsesHostProducerAndSurvivesShutdown)
{
    bool populated = false;
    module.SetSceneSource([&](FramePacket &packet, SceneId scene) {
        if (!populated) {
            AddHostMesh(packet, scene, renderer->GetInfo());
            packet.commands.push_back(CreateViewport{{90, 1}, {ViewportKind::OFFSCREEN, {32, 32}}});
            ViewDesc view;
            view.scene  = scene;
            view.output = {90, 1};
            packet.commands.push_back(CreateView{{90, 1}, view});
            populated = true;
        }
        packet.readbacks.push_back({90, 1});
    });
    module.Start();
    ASSERT_TRUE(Until([&] {
        for (const auto &event : events)
            if (const auto *pixels = std::get_if<ViewportReadback>(&event); pixels && pixels->viewport == ViewportId{90, 1}) {
                const size_t center = (16 * 32 + 16) * 4;
                if (pixels->bytes[center] > 240 && pixels->bytes[center + 1] < 10)
                    return true;
            }
        module.Tick(1.f / 60);
        return false;
    }));
    const auto handle = static_cast<HWND>(window->GetNativeHandle());
    SetWindowPos(handle, nullptr, 0, 0, 200, 150, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    module.Tick(1.f / 60);
    SendMessageW(handle, WM_CLOSE, 0, 0);
    module.Tick(1.f / 60);
    module.Shutdown();
    EXPECT_TRUE(IsWindow(handle));
    EXPECT_FALSE(failed);
}
