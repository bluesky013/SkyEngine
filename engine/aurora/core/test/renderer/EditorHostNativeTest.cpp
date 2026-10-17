//
// Created on 2026/10/17.
//
#include "HostSceneFixture.h"
#include <aurora/adaptor/AuroraModule.h>
#include <aurora/rhi/Instance.h>
#include <chrono>
#include <editor/render/EditorRenderer.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindow.h>
#include <thread>

#include <D3D12Device.h>
#include <d3d12sdklayers.h>
#include <windows.h>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;
namespace {
    class EditorHostNativeTest : public testing::TestWithParam<uint32_t>, public ISystemNotify {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(Platform::Get()->Init({}));
            window.reset(NativeWindow::Create({160, 120, "B14EditorHost", "Editor host test", nullptr}));
            ASSERT_TRUE(window);
            Interface<ISystemNotify>::Get()->Register(*this);
            modules.AddStaticModule("SandboxModule", std::make_unique<IModule>());
            modules.TopoSort();
            modules.LoadModules({});
            StartArguments args;
            args.args = {"EditorHostTest", "--aurora-provider-only"};
            ASSERT_TRUE(provider.Init(args));
            ASSERT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
            ASSERT_TRUE(editor.Init("B14", 160, 120, GetParam() == 2 ? API::DX12 : API::VULKAN, false,
                                    GetParam() == 1 ? RendererVulkanMode::RENDER_PASS : RendererVulkanMode::DYNAMIC_RENDERING));
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
            editor.Shutdown();
            provider.Shutdown();
            modules.UnLoadModules();
            EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
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
            Interface<ISystemNotify>::Get()->UnRegister();
            window.reset();
            Platform::Get()->Shutdown();
            Instance::Destroy();
        }
        ModuleManager *GetModuleManager() const override
        {
            return const_cast<ModuleManager *>(&modules);
        }
        void *GetMainWindowHandle() const override
        {
            return window->GetNativeHandle();
        }
        void SetExit() override
        {
            failed = true;
        }
        template <typename Predicate>
        bool Until(Predicate done)
        {
            const auto end = std::chrono::steady_clock::now() + 10s;
            do {
                bool quit = false;
                Platform::Get()->PoolEvent(quit);
                if (done())
                    return true;
                editor.Tick(1.f / 60);
                std::this_thread::sleep_for(2ms);
            } while (std::chrono::steady_clock::now() < end);
            return false;
        }
        ModuleManager                           modules;
        AuroraModule                            provider;
        editor::EditorRenderer                  editor;
        std::unique_ptr<NativeWindow>           window;
        IRenderer                              *renderer = nullptr;
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> debug;
        bool                                    failed = false;
    };
} // namespace
TEST_P(EditorHostNativeTest, B14SceneUiAndToolWindowRetireOnOneRenderer)
{
    uint32_t painted = 0;
    editor.SetGuiSource([&](uint32_t, ui::UIPaintContext &paint, uint32_t w, uint32_t h) {
        ++painted;
        paint.AddRect({0, 0, float(w), float(h)}, 0xFF101010);
        paint.AddRoundedRect({8, 8, 64, 48}, 0xFF00FF00, 8);
        paint.AddTexturedQuad({64, 8, float(w), float(h)}, {0, 0, 1, 1}, 1, 0xFFFFFFFF);
    });
    bool populated = false;
    editor.SetSceneSource([&](FramePacket &packet, SceneId scene) {
        if (!populated) {
            AddHostMesh(packet, scene, renderer->GetInfo());
            populated = true;
        }
        packet.readbacks.push_back({0, 1});
    });
    editor.Start();
    const auto tool = editor.CreateSurface("B14 Tool", 0, 0, 128, 96);
    ASSERT_NE(tool, 0u);
    auto toolWindow = FindWindowA(nullptr, "B14 Tool");
    ASSERT_TRUE(toolWindow);
    ASSERT_TRUE(Until([&] {
        std::array<RendererEvent, 32> events;
        const auto                    count = renderer->PollEvents(events);
        for (const auto &event : std::span(events).first(count)) {
            EXPECT_FALSE(std::holds_alternative<RendererFailure>(event));
            if (const auto *pixels = std::get_if<ViewportReadback>(&event)) {
                const size_t center = (90 * 320 + 160) * 4;
                if (pixels->bytes[center] > 240 && pixels->bytes[center + 1] < 10)
                    return true;
            }
        }
        return false;
    }));
    EXPECT_GT(painted, 1u);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), renderer);
    SetWindowPos(toolWindow, nullptr, 0, 0, 192, 144, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessageW(toolWindow, WM_CLOSE, 0, 0);
    EXPECT_TRUE(IsWindow(toolWindow));
    ASSERT_TRUE(Until([&] { return !IsWindow(toolWindow); }));
    const auto main = static_cast<HWND>(window->GetNativeHandle());
    SetWindowPos(main, nullptr, 0, 0, 240, 180, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    editor.Tick(1.f / 60);
    SendMessageW(main, WM_CLOSE, 0, 0);
    EXPECT_TRUE(IsWindow(main));
    editor.Shutdown();
    EXPECT_TRUE(IsWindow(main));
    EXPECT_FALSE(window->IsCloseDeferred());
    EXPECT_FALSE(failed);
}
INSTANTIATE_TEST_SUITE_P(Backends, EditorHostNativeTest, testing::Values(0u, 1u, 2u));
