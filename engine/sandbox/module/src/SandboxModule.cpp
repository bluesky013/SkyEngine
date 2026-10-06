//
// Created on 2026/09/21.
//
// Thin editor module: delegates to the editor-owned renderer (EditorRender).
// Runs in hub mode (Project Manager) when no `--project` is given.
//

#include <editor/sandbox/SandboxModule.h>
#include <editor/sandbox/UiIconBuilder.h>

#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>
#include <editor/core/extension/DefaultEditorExtension.h>
#include <editor/core/layout/LayoutPersistence.h>
#include <editor/core/property/EditorPropertySource.h>
#include <editor/core/resource/SandboxResources.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/DerivedDataCache.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindowManager.h>
#include <ui/UILayout.h>

#include <core/file/FileSystem.h>

#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#endif

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

        const char *ApiName(sky::aurora::API api)
        {
            switch (api) {
                case sky::aurora::API::VULKAN: return "Vulkan";
                case sky::aurora::API::DX12: return "DX12";
                case sky::aurora::API::METAL: return "Metal";
                default: return "Auto";
            }
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

        std::string ParseProjectArg(const sky::StartArguments &args)
        {
            if (args.args.empty()) {
                return {};
            }
            sky::CmdOptions options("SandboxEditor", "SkyEngine Editor");
            options.allow_unrecognised_options();
            options.add_options()("p,project", "Project (.skyproj)", sky::CmdValue<std::string>());

            auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
            if (result.count("project") != 0u) {
                return result["project"].as<std::string>();
            }
            return {};
        }
        class HubTarget : public IUiTarget {
        public:
            explicit HubTarget(ProjectManagerView *view) : view(view) {}
            void OnPointer(const sky::ui::UIPointerEvent &event) override
            {
                if (view != nullptr) {
                    view->OnPointerEvent(event);
                }
            }
            bool WantsInput() const override { return view != nullptr; }

        private:
            ProjectManagerView *view = nullptr;
        };

        class ShellTarget : public IUiTarget {
        public:
            explicit ShellTarget(EditorShell *shell) : shell(shell) {}
            void OnPointer(const sky::ui::UIPointerEvent &event) override
            {
                if (shell != nullptr) {
                    shell->DispatchPointer(event);
                }
            }
            void OnKey(const sky::ui::UIKeyEvent &event) override
            {
                if (shell != nullptr) {
                    shell->DispatchKey(event);
                }
            }
            void OnText(const sky::ui::UITextInputEvent &event) override
            {
                if (shell != nullptr) {
                    shell->DispatchText(event);
                }
            }
            bool WantsInput() const override { return shell != nullptr && shell->WantsInput(); }

        private:
            EditorShell *shell = nullptr;
        };
    } // namespace

    std::unique_ptr<IUiTarget> MakeHubTarget(ProjectManagerView *view)
    {
        return std::make_unique<HubTarget>(view);
    }

    std::unique_ptr<IUiTarget> MakeShellTarget(EditorShell *shell)
    {
        return std::make_unique<ShellTarget>(shell);
    }

    SandboxModule::SandboxModule() = default;
    SandboxModule::~SandboxModule() = default;

    bool SandboxModule::Init(const sky::StartArguments &args)
    {
        const auto api = ParseApiArgs(args);
        rhiName = ApiName(api);
        const std::string projectPath = ParseProjectArg(args);
        hubMode = projectPath.empty();

        // Editor mode: read the project and take the single-instance lock BEFORE
        // any window/GPU work, so a second editor on the same project refuses
        // cheaply (ModuleManager ignores a failed Init, so also request exit).
        if (!hubMode) {
            project.Read(projectPath);
            if (!projectLock.Acquire(project.Dir())) {
                LOG_E(TAG, "project already open in another editor: %s", project.Dir().c_str());
                if (auto *system = Interface<ISystemNotify>::Get()->GetApi()) {
                    system->SetExit();
                }
                return false;
            }
        }

        // The separate preview window is not created: the design makes the
        // preview a docked panel by default (floating/tear-out is reserved), and
        // docking is not implemented yet, so no standalone window is opened.
        if (!renderer.Init("SandboxEditor", 1280, 720, api, /*withPreview*/ false)) {
            return false;
        }
        initialized = true;

        // DPI scale = dpi / 96 (96 = 100%).
        uiScale = 1.0f;
#if defined(_WIN32)
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi()) {
            if (void *handle = system->GetMainWindowHandle()) {
                const UINT dpi = ::GetDpiForWindow(static_cast<HWND>(handle));
                if (dpi > 0) {
                    uiScale = static_cast<float>(dpi) / 96.0f;
                }
            }
        }
#endif
        if (const char *env = std::getenv("SKY_UI_SCALE")) {
            const float value = static_cast<float>(std::atof(env));
            if (value >= 0.5f && value <= 4.0f) {
                uiScale = value;
            }
        }

        // No project -> hub (Project Manager); otherwise the editor shell.
        bool ok = false;
        if (hubMode) {
            ok = BuildHub();
        } else {
            ProjectRegistry::Get()->Add(projectPath);
            ok = BuildEditor();
        }
        if (!ok) {
            return false;
        }

        renderer.SetGuiSource([this](uint32_t /*surfaceId*/, sky::ui::UIPaintContext &context, uint32_t w, uint32_t h) {
            PaintGui(context, w, h);
        });

        mouseBinder.Bind(this);
        keyBinder.Bind(this);
        return true;
    }

    bool SandboxModule::BuildHub()
    {
        ProjectRegistry::Get()->Load();

        hubContext = std::make_unique<sky::ui::UIContext>();
        auto view = std::make_unique<ProjectManagerView>();
        hubView = view.get();
        hubTarget = MakeHubTarget(hubView);
        hubView->SetTextSystem(renderer.GetTextSystem());
        // Fill the whole window (default layout is a zero-size point anchor).
        sky::ui::UILayoutParams fill;
        fill.anchorMinX = 0.0f;
        fill.anchorMaxX = 1.0f;
        fill.anchorMinY = 0.0f;
        fill.anchorMaxY = 1.0f;
        view->SetLayout(fill);
        hubView->onOpen   = [this](const std::string &path) { OpenProject(path); };
        hubView->onAdd    = [this]() { AddProject(); };
        hubView->onNew    = [this]() { NewProject(); };
        hubView->onRemove = [this](const std::string &path) { RemoveFromList(path); };
        hubView->onDelete = [this](const std::string &path) { DeleteProjectFolder(path); };
        hubView->onQuit   = [this]() {
            if (auto *system = Interface<ISystemNotify>::Get()->GetApi()) {
                system->SetExit();
            }
        };
        hubView->SetEngineVersion(sky::kEngineVersion);
        hubContext->AddChild(std::move(view));
        RefreshHubRecent();
        LOG_I(TAG, "project manager (hub) mode");
        return true;
    }

    bool SandboxModule::BuildEditor()
    {
        // Mount the project workspace (writable) + engine bundle (read-only) into
        // the authoring asset DB, mirroring the legacy editor. Paths are relative
        // to each side's `assets/` root.
        if (!project.Dir().empty()) {
            auto *workFs = new sky::NativeFileSystem(project.Dir());
            auto *engineFs = new sky::NativeFileSystem(Platform::Get()->GetBundlePath());
            sky::AssetDataBase::Get()->SetEngineFs(engineFs);
            sky::AssetDataBase::Get()->SetWorkSpaceFs(workFs->CreateSubSystem("assets", true));
            sky::AssetManager::Get()->SetWorkFileSystem(workFs);
        }

        // EditorCore services + default panels (registered through the extension
        // host) + default layout.
        extensionHost.Add(std::make_unique<DefaultEditorExtension>(panelRegistry));
        extensionHost.RegisterAll();

        // Default: Outliner (left) | Viewport/OutputLog (center) | Inspector (right).
        // The Reflection Demo panel is not shown by default; it opens from
        // Help > Demo (see EditorShell).
        layoutModel.SetDefault({"outliner"});
        layoutModel.SplitPanel("outliner", SplitOrientation::HORIZONTAL, "viewport");
        layoutModel.SplitPanel("viewport", SplitOrientation::HORIZONTAL, "inspector");
        layoutModel.SplitPanel("viewport", SplitOrientation::VERTICAL, "outputlog");
        layoutModel.Tabify("config", "outliner");
        layoutModel.Tabify("console", "outputlog");

        // SplitPanel nests by halving, which leaves the Outliner too wide. Set
        // explicit ratios for a balanced UE/Blender-like layout.
        if (auto *rootSplit = static_cast<SplitNode *>(layoutModel.GetRoot())) {
            if (rootSplit->children.size() == 2 && IsSplit(rootSplit->children[1].get())) {
                rootSplit->ratios = {0.22f, 0.78f}; // outliner | rest
                auto *inner = static_cast<SplitNode *>(rootSplit->children[1].get());
                if (inner->children.size() == 2) {
                    inner->ratios = {0.68f, 0.32f}; // (viewport/outputlog) | inspector
                }
            }
        }

        // Per-user layout: restore a saved arrangement when present/valid, else
        // keep the default built above.
        layoutPath = LayoutPersistence::GetDefaultPath();
        if (!layoutPath.empty()) {
            std::vector<std::string> layoutWarnings;
            if (LayoutPersistence::Load(layoutPath, layoutModel, &panelRegistry, &layoutWarnings)) {
                LOG_I(TAG, "restored editor layout from %s", layoutPath.c_str());
            }
        }

        shell.SetTextSystem(renderer.GetTextSystem());
        shell.SetLayout(&layoutModel);
        shell.SetPanelRegistry(&panelRegistry);
        shell.SetSelection(&selection);
        static RegisteredPropertySource propertySource;
        shell.SetPropertySource(&propertySource);
        shell.SetUiScale(uiScale);
        shell.SetStatusInfo(project.name, rhiName, "Edit");
        // A closed floating window re-docks its panel into the main window.
        renderer.SetSurfaceClosedCallback([this](const std::string &panelId) {
            shell.DockFloatingPanel(panelId, "viewport", DockPosition::CENTER);
        });
        renderer.SetSurfaceGeometryCallback([this](const std::string &panelId, float x, float y, float w, float h) {
            shell.SetFloatingGeometry(panelId, x, y, w, h);
        });

        sky::DerivedDataCache::Get().SetRoot(SandboxResources::Resolve("cache"));
        InstallUiIconBuilder();
        shell.SetLogService(&logService);
        shell.SetCommandController(&commandController);
        shell.RegisterBuiltinPanelViews();
        shellTarget = MakeShellTarget(&shell);
        shell.Rebuild();

        // Materialize floating windows restored from the saved layout.
        for (const FloatingPanel &fp : layoutModel.GetFloatingPanels()) {
            const uint32_t fw = fp.width > 1.0f ? static_cast<uint32_t>(fp.width) : 720u;
            const uint32_t fh = fp.height > 1.0f ? static_cast<uint32_t>(fp.height) : 480u;
            const uint32_t surfaceId =
                renderer.CreateSurface(fp.panelId, static_cast<int>(fp.x), static_cast<int>(fp.y), fw, fh);
            if (surfaceId != 0) {
                shell.SetSurfaceScale(surfaceId, renderer.SurfaceDpiScale(surfaceId));
                shell.RestoreFloatingPanel(fp.panelId, surfaceId);
            }
        }

        logService.Install();
        LOG_I(TAG, "editor mode: project '%s'", project.name.c_str());
        return true;
    }

    bool SandboxModule::ValidateProject(const ProjectDescriptor &descriptor, std::string &message)
    {
        if (descriptor.engineVersion.empty() || descriptor.engineVersion == sky::kEngineVersion) {
            return true;
        }
        if (descriptor.engineVersion > std::string(sky::kEngineVersion)) {
            message = "Project requires engine " + descriptor.engineVersion + " (running " + sky::kEngineVersion + ")";
            return false;
        }
        message = "Project made with " + descriptor.engineVersion + "; running " + sky::kEngineVersion;
        return true;
    }

    void SandboxModule::OpenProject(const std::string &skyprojPath)
    {
        if (!project.Read(skyprojPath)) {
            if (hubView != nullptr) {
                hubView->SetStatus("Failed to open: " + skyprojPath);
            }
            return;
        }
        std::string message;
        if (!ValidateProject(project, message)) {
            if (hubView != nullptr) {
                hubView->SetStatus(message);
            }
            return;
        }
        if (!projectLock.Acquire(project.Dir())) {
            if (hubView != nullptr) {
                hubView->SetStatus("Project is already open in another editor: " + project.name);
            }
            return;
        }
        ProjectRegistry::Get()->Add(skyprojPath);

        // v1: switch in-process from hub to the editor shell (re-exec is the
        // target design; this keeps the first version simple and inspectable).
        hubView = nullptr;
        hubContext.reset();
        hubMode = false;
        BuildEditor();
    }

    void SandboxModule::AddProject()
    {
#if defined(_WIN32)
        if (hubView == nullptr) {
            return;
        }
        wchar_t file[MAX_PATH] = L"";
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = nullptr;
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi()) {
            ofn.hwndOwner = static_cast<HWND>(system->GetMainWindowHandle());
        }
        ofn.lpstrFilter = L"SkyEngine Project (*.skyproj)\0*.skyproj\0All Files\0*.*\0\0";
        ofn.lpstrFile   = file;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (::GetOpenFileNameW(&ofn) == FALSE) {
            return;
        }
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
        std::string path(size > 0 ? size - 1 : 0, '\0');
        if (size > 0) {
            ::WideCharToMultiByte(CP_UTF8, 0, file, -1, path.data(), size, nullptr, nullptr);
        }
        ProjectDescriptor descriptor;
        if (!descriptor.Read(path)) {
            hubView->SetStatus("Not a valid project: " + path);
            return;
        }
        std::string message;
        if (!ValidateProject(descriptor, message)) {
            hubView->SetStatus(message);
            return;
        }
        ProjectRegistry::Get()->Add(path);
        RefreshHubRecent();
        hubView->SetStatus(message.empty() ? ("Added " + descriptor.name) : message);
#else
        if (hubView != nullptr) {
            hubView->SetStatus("Add Project is not available on this platform yet");
        }
#endif
    }

    void SandboxModule::RemoveFromList(const std::string &skyprojPath)
    {
        ProjectRegistry::Get()->Remove(skyprojPath);
        RefreshHubRecent();
        if (hubView != nullptr) {
            hubView->SetStatus("Removed from list");
        }
    }

    void SandboxModule::DeleteProjectFolder(const std::string &skyprojPath)
    {
        ProjectDescriptor descriptor;
        const std::string dir = descriptor.Read(skyprojPath)
                                    ? descriptor.Dir()
                                    : std::filesystem::path(skyprojPath).parent_path().string();
        std::error_code error;
        std::filesystem::remove_all(dir, error);
        ProjectRegistry::Get()->Remove(skyprojPath);
        RefreshHubRecent();
        if (hubView != nullptr) {
            hubView->SetStatus(error ? ("Failed to delete: " + dir) : ("Deleted " + dir));
        }
    }

    void SandboxModule::NewProject()
    {
        std::string base = Platform::Get() != nullptr ? Platform::Get()->GetUserConfigPath() : std::string{};
        if (base.empty()) {
            base = ".";
        }
        const std::string projectsDir = (std::filesystem::path(base) / "skyengine" / "projects").string();

        // Unique "MyProject", "MyProject2", ... under the user projects dir.
        std::string name = "MyProject";
        std::string skyproj;
        for (int i = 1; i < 1000; ++i) {
            name = (i == 1) ? "MyProject" : ("MyProject" + std::to_string(i));
            if (!std::filesystem::exists(std::filesystem::path(projectsDir) / name)) {
                break;
            }
        }

        if (ProjectDescriptor::Create(projectsDir, name, skyproj)) {
            ProjectRegistry::Get()->Add(skyproj);
            RefreshHubRecent();
            if (hubView != nullptr) {
                hubView->SetStatus("Created " + skyproj);
            }
            LOG_I(TAG, "created project '%s'", skyproj.c_str());
        } else if (hubView != nullptr) {
            hubView->SetStatus("Failed to create project under " + projectsDir);
        }
    }

    void SandboxModule::RefreshHubRecent()
    {
        if (hubView != nullptr) {
            hubView->SetRecent(ProjectRegistry::Get()->Recent());
        }
    }

    void SandboxModule::PaintGui(sky::ui::UIPaintContext &context, uint32_t width, uint32_t height)
    {
        if (hubMode && hubContext != nullptr) {
            hubContext->SetContentSize(static_cast<float>(width), static_cast<float>(height));
            hubContext->Layout();
            hubContext->Paint(context);
            return;
        }
        shell.Layout(static_cast<float>(width), static_cast<float>(height));
        shell.Paint(context);
    }

    void SandboxModule::Start()
    {
        if (initialized) {
            renderer.Start();
        }
    }

    void SandboxModule::Tick(float delta)
    {
        if (!initialized) {
            return;
        }
        if (!hubMode) {
            logService.Pump();

            // Tear-out: a tab dragged out of the dock area floats into its own
            // window (created here, then bound to the shell's per-window context).
            std::string floatPanel;
            if (shell.ConsumeFloatRequest(floatPanel)) {
                const uint32_t surfaceId = renderer.CreateSurface(floatPanel, 200, 200, 720, 480);
                if (surfaceId != 0) {
                    shell.SetSurfaceScale(surfaceId, renderer.SurfaceDpiScale(surfaceId));
                    shell.FloatPanelToSurface(floatPanel, surfaceId, FloatingPanel{});
                }
            }
        }
        renderer.Tick(delta);

        // Coalesced end-of-frame auto-save: only after a committed layout edit.
        if (!hubMode && shell.IsBuilt() && shell.ConsumeLayoutDirty() && !layoutPath.empty()) {
            LayoutPersistence::Save(layoutModel, layoutPath);
        }
    }

    bool SandboxModule::AcceptWindowEvent(sky::WindowID winID)
    {
        if (winID == 0) {
            return true; // id-less event: fall back to the active target
        }
        if (primaryWindowId == 0) {
            primaryWindowId = winID; // learn the main window from the first event
        }
        return winID == primaryWindowId;
    }

    void SandboxModule::RoutePointer(const sky::ui::UIPointerEvent &pointer, sky::WindowID winID)
    {
        if (AcceptWindowEvent(winID)) {
            if (auto *target = ActiveTarget()) {
                target->OnPointer(pointer);
            }
        } else {
            RouteFloatingPointer(winID, pointer);
        }
    }

    bool SandboxModule::RouteFloatingPointer(sky::WindowID winID, const sky::ui::UIPointerEvent &pointer)
    {
        auto *window = sky::NativeWindowManager::Get()->GetWindowByID(winID);
        if (window == nullptr) {
            return false;
        }
        const uint32_t surfaceId = renderer.SurfaceIdForWindow(window);
        if (surfaceId == 0) {
            return false;
        }
        shell.DispatchPointerToSurface(surfaceId, pointer);
        return true;
    }

    void SandboxModule::OnMouseButtonDown(const sky::MouseButtonEvent &event)
    {
        sky::ui::UIPointerEvent pointer;
        pointer.action = sky::ui::UIPointerAction::DOWN;
        pointer.button = static_cast<uint32_t>(event.button);
        pointer.x      = static_cast<float>(event.x);
        pointer.y      = static_cast<float>(event.y);
        RoutePointer(pointer, event.winID);
        if (!hubMode) {
            // Begin a drag: take OS pointer capture so motion keeps arriving even
            // when the cursor leaves the window (main and floating alike).
            if (auto *window = sky::NativeWindowManager::Get()->GetWindowByID(event.winID)) {
                window->SetPointerCapture(true);
            }
        }
    }

    void SandboxModule::OnMouseButtonUp(const sky::MouseButtonEvent &event)
    {
        sky::ui::UIPointerEvent pointer;
        pointer.action = sky::ui::UIPointerAction::UP;
        pointer.button = static_cast<uint32_t>(event.button);
        pointer.x      = static_cast<float>(event.x);
        pointer.y      = static_cast<float>(event.y);
        RoutePointer(pointer, event.winID);
        if (!hubMode) {
            if (auto *window = sky::NativeWindowManager::Get()->GetWindowByID(event.winID)) {
                window->SetPointerCapture(false);
            }
        }
    }

    void SandboxModule::OnMouseMotion(const sky::MouseMotionEvent &event)
    {
        sky::ui::UIPointerEvent pointer;
        pointer.action = sky::ui::UIPointerAction::MOVE;
        pointer.x      = static_cast<float>(event.x);
        pointer.y      = static_cast<float>(event.y);
        RoutePointer(pointer, event.winID);

        // Reflect the hovered element's cursor (e.g. a resize cursor on a
        // splitter) on the window under the pointer.
        if (!hubMode) {
            if (auto *window = sky::NativeWindowManager::Get()->GetWindowByID(event.winID)) {
                const uint32_t surfaceId = renderer.SurfaceIdForWindow(window);
                window->SetCursor(shell.DesiredCursor(surfaceId));
            }
        }
    }

    void SandboxModule::OnMouseWheel(const sky::MouseWheelEvent &event)
    {
        if (hubMode) {
            return;
        }
        sky::ui::UIPointerEvent pointer;
        pointer.action     = sky::ui::UIPointerAction::WHEEL;
        pointer.x          = static_cast<float>(event.x);
        pointer.y          = static_cast<float>(event.y);
        pointer.wheelDelta = static_cast<float>(event.y);
        RoutePointer(pointer, event.winID);
    }

    void SandboxModule::OnKeyDown(const sky::KeyboardEvent &event)
    {
        if (!AcceptWindowEvent(event.winID)) {
            return;
        }
        sky::ui::UIKeyEvent key;
        key.keyCode   = static_cast<uint32_t>(event.scanCode);
        key.action    = sky::ui::UIKeyAction::DOWN;
        key.modifiers = static_cast<uint32_t>(event.mod);
        if (auto *target = ActiveTarget()) {
            target->OnKey(key);
        }
    }

    void SandboxModule::OnKeyUp(const sky::KeyboardEvent &event)
    {
        if (!AcceptWindowEvent(event.winID)) {
            return;
        }
        sky::ui::UIKeyEvent key;
        key.keyCode   = static_cast<uint32_t>(event.scanCode);
        key.action    = sky::ui::UIKeyAction::UP;
        key.modifiers = static_cast<uint32_t>(event.mod);
        if (auto *target = ActiveTarget()) {
            target->OnKey(key);
        }
    }

    void SandboxModule::OnTextInput(sky::WindowID winID, const char *text)
    {
        if (text == nullptr || !AcceptWindowEvent(winID)) {
            return;
        }
        sky::ui::UITextInputEvent input;
        input.text = text;
        if (auto *target = ActiveTarget()) {
            target->OnText(input);
        }
    }

    void SandboxModule::Shutdown()
    {
        // Flush any pending layout edit before teardown.
        if (!hubMode && shell.IsBuilt() && shell.ConsumeLayoutDirty() && !layoutPath.empty()) {
            LayoutPersistence::Save(layoutModel, layoutPath);
        }
        mouseBinder.Reset();
        keyBinder.Reset();
        projectLock.Release();
        hubView = nullptr;
        hubContext.reset();
        if (!hubMode) {
            extensionHost.UnregisterAll();
            logService.Uninstall();
        }
        if (initialized) {
            renderer.Shutdown();
        }
        initialized = false;
    }

} // namespace sky::editor
