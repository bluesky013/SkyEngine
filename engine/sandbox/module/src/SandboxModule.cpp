//
// Created on 2026/09/21.
//
// Thin editor module: delegates to the editor-owned renderer (EditorRender).
//

#include <editor/sandbox/SandboxModule.h>

#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>
#include <editor/core/extension/DefaultEditorExtension.h>

static const char *TAG = "SandboxModule";

namespace sky::editor {

    namespace {
        sky::aurora::API ParseApi(const std::string &name)
        {
            if (name == "vulkan" || name == "vk") {
                return sky::aurora::API::VULKAN;
            }
            if (name == "dx12" || name == "d3d12") {
                return sky::aurora::API::DX12;
            }
            if (name == "metal") {
                return sky::aurora::API::METAL;
            }
            return sky::aurora::API::DEFAULT;
        }

        sky::aurora::API ParseApiArgs(const sky::StartArguments &args)
        {
            if (args.args.empty()) {
                return sky::aurora::API::DEFAULT;
            }
            sky::CmdOptions options("SandboxEditor", "SkyEngine Editor");
            options.allow_unrecognised_options();
            options.add_options()("r,rhi", "RHI Type", sky::CmdValue<std::string>());

            auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
            if (result.count("rhi") != 0u) {
                return ParseApi(result["rhi"].as<std::string>());
            }
            return sky::aurora::API::DEFAULT;
        }
    } // namespace

    SandboxModule::SandboxModule() = default;
    SandboxModule::~SandboxModule() = default;

    bool SandboxModule::Init(const sky::StartArguments &args)
    {
        const auto api = ParseApiArgs(args);
        initialized = renderer.Init("SandboxEditor", 1280, 720, api);
        if (!initialized) {
            return false;
        }

        // EditorCore services + default panels (registered through the extension
        // host) + default layout.
        extensionHost.Add(std::make_unique<DefaultEditorExtension>(panelRegistry));
        extensionHost.RegisterAll();

        layoutModel.SetDefault({"outliner"});
        layoutModel.SplitPanel("outliner", SplitOrientation::HORIZONTAL, "viewport");
        layoutModel.SplitPanel("viewport", SplitOrientation::VERTICAL, "outputlog");
        layoutModel.Tabify("inspector", "outliner");
        layoutModel.Tabify("console", "outputlog");

        // UI-linked shell composed from the layout + registry.
        shell.SetTextSystem(renderer.GetTextSystem());
        shell.SetLayout(&layoutModel);
        shell.SetPanelRegistry(&panelRegistry);
        shell.SetSelection(&selection);
        shell.SetLogService(&logService);
        shell.SetCommandController(&commandController);
        shell.RegisterBuiltinPanelViews();
        shell.Rebuild();

        // Capture engine log output for the Output Log panel.
        logService.Install();

        // Receive platform input and forward it to the shell.
        mouseBinder.Bind(this);
        keyBinder.Bind(this);

        // The renderer draws whatever the shell produces each frame.
        renderer.SetGuiSource([this](sky::ui::UIPaintContext &context, uint32_t w, uint32_t h) {
            shell.Layout(static_cast<float>(w), static_cast<float>(h));
            shell.Paint(context);
        });
        return true;
    }

    void SandboxModule::Start()
    {
        if (initialized) {
            renderer.Start();
        }
    }

    void SandboxModule::Tick(float delta)
    {
        if (initialized) {
            logService.Pump(); // refresh the filtered log view for the panel
            renderer.Tick(delta);
        }
    }

    void SandboxModule::OnMouseButtonDown(const sky::MouseButtonEvent &event)
    {
        sky::ui::UIPointerEvent pointer;
        pointer.action = sky::ui::UIPointerAction::DOWN;
        pointer.button = static_cast<uint32_t>(event.button);
        pointer.x      = static_cast<float>(event.x);
        pointer.y      = static_cast<float>(event.y);
        shell.DispatchPointer(pointer);
    }

    void SandboxModule::OnMouseButtonUp(const sky::MouseButtonEvent &event)
    {
        sky::ui::UIPointerEvent pointer;
        pointer.action = sky::ui::UIPointerAction::UP;
        pointer.button = static_cast<uint32_t>(event.button);
        pointer.x      = static_cast<float>(event.x);
        pointer.y      = static_cast<float>(event.y);
        shell.DispatchPointer(pointer);
    }

    void SandboxModule::OnMouseMotion(const sky::MouseMotionEvent &event)
    {
        sky::ui::UIPointerEvent pointer;
        pointer.action = sky::ui::UIPointerAction::MOVE;
        pointer.x      = static_cast<float>(event.x);
        pointer.y      = static_cast<float>(event.y);
        shell.DispatchPointer(pointer);
    }

    void SandboxModule::OnMouseWheel(const sky::MouseWheelEvent &event)
    {
        sky::ui::UIPointerEvent pointer;
        pointer.action     = sky::ui::UIPointerAction::WHEEL;
        pointer.x          = static_cast<float>(event.x);
        pointer.y          = static_cast<float>(event.y);
        pointer.wheelDelta = static_cast<float>(event.y);
        shell.DispatchPointer(pointer);
    }

    void SandboxModule::OnKeyDown(const sky::KeyboardEvent &event)
    {
        sky::ui::UIKeyEvent key;
        key.keyCode   = static_cast<uint32_t>(event.scanCode);
        key.action    = sky::ui::UIKeyAction::DOWN;
        key.modifiers = static_cast<uint32_t>(event.mod);
        shell.DispatchKey(key);
    }

    void SandboxModule::OnKeyUp(const sky::KeyboardEvent &event)
    {
        sky::ui::UIKeyEvent key;
        key.keyCode   = static_cast<uint32_t>(event.scanCode);
        key.action    = sky::ui::UIKeyAction::UP;
        key.modifiers = static_cast<uint32_t>(event.mod);
        shell.DispatchKey(key);
    }

    void SandboxModule::OnTextInput(sky::WindowID /*winID*/, const char *text)
    {
        sky::ui::UITextInputEvent input;
        input.text = text != nullptr ? text : "";
        shell.DispatchText(input);
    }

    void SandboxModule::Shutdown()
    {
        mouseBinder.Reset();
        keyBinder.Reset();
        extensionHost.UnregisterAll();
        logService.Uninstall();
        if (initialized) {
            renderer.Shutdown();
        }
        initialized = false;
    }

} // namespace sky::editor
