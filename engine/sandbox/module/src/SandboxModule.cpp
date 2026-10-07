//
// Created on 2026/09/21.
//
// Thin editor module: delegates to the editor-owned renderer (EditorRender).
// Runs in hub mode (Project Manager) when no `--project` is given.
//

#include <editor/sandbox/SandboxModule.h>
#include <editor/sandbox/UiIconBuilder.h>

#include <core/cmdline/CmdParser.h>
#include <core/file/FileIO.h>
#include <core/logger/Logger.h>
#include <editor/core/EditorCore.h>
#include <editor/core/extension/DefaultEditorExtension.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/layout/LayoutPersistence.h>
#include <editor/core/property/EditorPropertySource.h>
#include <editor/core/resource/SandboxResources.h>
#include <editor/shell/UiTheme.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/DerivedDataCache.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindowManager.h>
#include <ui/IUITextureRegistry.h>
#include <ui/UILayout.h>
#include <ui/text/UITextSystem.h>

#include <core/file/FileSystem.h>

#include <filesystem>
#include <fstream>
#include <iterator>

#include <nanosvg/nanosvg.h>
#include <nanosvg/nanosvgrast.h>

#if defined(_WIN32)
    #include <windows.h>
#endif

static const char *TAG = "SandboxModule";

namespace sky::editor {

    namespace {
        // Rasterizes an editor SVG icon into an RGBA8 image (nanosvg).
        bool RasterizeSvg(const std::string &path, uint32_t size, sky::ui::UIImageData &out)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                return false;
            }
            std::string svg((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (svg.empty()) {
                return false;
            }
            NSVGimage *image = nsvgParse(svg.data(), "px", 96.0f);
            if (image == nullptr) {
                return false;
            }
            NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
            out.width                  = size;
            out.height                 = size;
            out.pixels.assign(static_cast<std::size_t>(size) * size * 4u, 0);
            const float scale = static_cast<float>(size) / std::max(image->width, image->height);
            nsvgRasterize(rasterizer, image, 0.0f, 0.0f, scale, out.pixels.data(), static_cast<int>(size), static_cast<int>(size),
                          static_cast<int>(size * 4u));
            nsvgDeleteRasterizer(rasterizer);
            nsvgDelete(image);
            return true;
        }

        // The UI widgets (EditBox, ReflectedFormView, FileBrowserDialog) expect
        // virtual-key codes. Map the platform ScanCode enum (framework/window/
        // IWindowEvent.h) to VK once here so every consumer sees the same keys.
        std::uint32_t ScanCodeToVirtualKey(sky::ScanCode code)
        {
            switch (code) {
            case sky::ScanCode::KEY_A: return 'A';
            case sky::ScanCode::KEY_B: return 'B';
            case sky::ScanCode::KEY_C: return 'C';
            case sky::ScanCode::KEY_D: return 'D';
            case sky::ScanCode::KEY_E: return 'E';
            case sky::ScanCode::KEY_F: return 'F';
            case sky::ScanCode::KEY_G: return 'G';
            case sky::ScanCode::KEY_H: return 'H';
            case sky::ScanCode::KEY_I: return 'I';
            case sky::ScanCode::KEY_J: return 'J';
            case sky::ScanCode::KEY_K: return 'K';
            case sky::ScanCode::KEY_L: return 'L';
            case sky::ScanCode::KEY_M: return 'M';
            case sky::ScanCode::KEY_N: return 'N';
            case sky::ScanCode::KEY_O: return 'O';
            case sky::ScanCode::KEY_P: return 'P';
            case sky::ScanCode::KEY_Q: return 'Q';
            case sky::ScanCode::KEY_R: return 'R';
            case sky::ScanCode::KEY_S: return 'S';
            case sky::ScanCode::KEY_T: return 'T';
            case sky::ScanCode::KEY_U: return 'U';
            case sky::ScanCode::KEY_V: return 'V';
            case sky::ScanCode::KEY_W: return 'W';
            case sky::ScanCode::KEY_X: return 'X';
            case sky::ScanCode::KEY_Y: return 'Y';
            case sky::ScanCode::KEY_Z: return 'Z';
            case sky::ScanCode::KEY_0: return '0';
            case sky::ScanCode::KEY_1: return '1';
            case sky::ScanCode::KEY_2: return '2';
            case sky::ScanCode::KEY_3: return '3';
            case sky::ScanCode::KEY_4: return '4';
            case sky::ScanCode::KEY_5: return '5';
            case sky::ScanCode::KEY_6: return '6';
            case sky::ScanCode::KEY_7: return '7';
            case sky::ScanCode::KEY_8: return '8';
            case sky::ScanCode::KEY_9: return '9';
            case sky::ScanCode::KEY_RETURN: return 0x0D;
            case sky::ScanCode::KEY_ESCAPE: return 0x1B;
            case sky::ScanCode::KEY_BACKSPACE: return 0x08;
            case sky::ScanCode::KEY_TAB: return 0x09;
            case sky::ScanCode::KEY_SPACE: return 0x20;
            case sky::ScanCode::KEY_LEFT: return 0x25;
            case sky::ScanCode::KEY_RIGHT: return 0x27;
            case sky::ScanCode::KEY_UP: return 0x26;
            case sky::ScanCode::KEY_DOWN: return 0x28;
            case sky::ScanCode::KEY_HOME: return 0x24;
            case sky::ScanCode::KEY_END: return 0x23;
            case sky::ScanCode::KEY_DELETE: return 0x2E;
            default: return 0;
            }
        }

        std::string DefaultProjectsDirectory()
        {
            std::string base = sky::Platform::Get() != nullptr ? sky::Platform::Get()->GetUserConfigPath() : std::string{};
            if (base.empty()) {
                base = ".";
            }
            return (std::filesystem::path(base) / "skyengine" / "projects").string();
        }

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
            explicit HubTarget(ProjectManagerView *view) : view(view)
            {
            }
            void OnPointer(const sky::ui::UIPointerEvent &event) override
            {
                if (view != nullptr) {
                    view->OnPointerEvent(event);
                }
            }
            bool WantsInput() const override
            {
                return view != nullptr;
            }

        private:
            ProjectManagerView *view = nullptr;
        };

        class ShellTarget : public IUiTarget {
        public:
            explicit ShellTarget(EditorShell *shell) : shell(shell)
            {
            }
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
            bool WantsInput() const override
            {
                return shell != nullptr && shell->WantsInput();
            }

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

    SandboxModule::SandboxModule()  = default;
    SandboxModule::~SandboxModule() = default;

    bool SandboxModule::Init(const sky::StartArguments &args)
    {
        const auto api                = ParseApiArgs(args);
        rhiName                       = ApiName(api);
        const std::string projectPath = ParseProjectArg(args);
        hubMode                       = projectPath.empty();

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

        // System DPI scale = dpi / 96 (96 = 100%); SKY_UI_SCALE overrides it for dev.
        systemUiScale = 1.0f;
#if defined(_WIN32)
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi()) {
            if (void *handle = system->GetMainWindowHandle()) {
                const UINT dpi = ::GetDpiForWindow(static_cast<HWND>(handle));
                if (dpi > 0) {
                    systemUiScale = static_cast<float>(dpi) / 96.0f;
                }
            }
        }
#endif
        if (const char *env = std::getenv("SKY_UI_SCALE")) {
            const float value = static_cast<float>(std::atof(env));
            if (value >= 0.5f && value <= 4.0f) {
                systemUiScale = value;
            }
        }
        uiScale = EffectiveUiScale();

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

        renderer.SetGuiSource([this](uint32_t /*surfaceId*/, sky::ui::UIPaintContext &context, uint32_t w, uint32_t h) { PaintGui(context, w, h); });

        mouseBinder.Bind(this);
        keyBinder.Bind(this);
        return true;
    }

    bool SandboxModule::BuildHub()
    {
        ProjectRegistry::Get()->Load();

        // The hub has no EditorShell, so apply the DPI/UI scale to the default
        // theme directly (the hub built its views at 1.0 before).
        SetDefaultUiTheme(MakeDarkTheme(EffectiveUiScale()));

        hubContext = std::make_unique<sky::ui::UIContext>();
        auto view  = std::make_unique<ProjectManagerView>();
        hubView    = view.get();
        hubTarget  = MakeHubTarget(hubView);
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

        auto dialog = std::make_unique<FileBrowserDialog>(renderer.GetTextSystem());
        browser     = dialog.get();
        dialog->SetLayout(fill);
        dialog->SetOnResult([this](const FileBrowserResult &result) { OnFileBrowserResult(result); });
        hubContext->AddChild(std::move(dialog));

        RefreshHubRecent();
        if (auto *window = sky::NativeWindowManager::Get()->GetMainWindow()) {
            window->SetTitle("Project Manager - SkyEngine Editor");
        }
        LOG_I(TAG, "project manager (hub) mode");
        return true;
    }

    std::string SandboxModule::WorldBasePath() const
    {
        if (!project.Dir().empty()) {
            return (std::filesystem::path(project.Dir()) / "assets").string();
        }
        const std::string cfg = Platform::Get() != nullptr ? Platform::Get()->GetUserConfigPath() : std::string{};
        return cfg.empty() ? std::string(".") : (std::filesystem::path(cfg) / "skyengine").string();
    }

    float SandboxModule::EffectiveUiScale() const
    {
        double pref = 1.0;
        if (preferenceStore != nullptr) {
            preferenceStore->GetFloat("editor.uiScale", pref);
        }
        const float scale = systemUiScale * static_cast<float>(pref);
        return scale < 0.5f ? 0.5f : (scale > 4.0f ? 4.0f : scale);
    }

    void SandboxModule::NewWorld()
    {
        StopPlay();
        // Open the "New World" create dialog (name + location).
        shell.OpenNewWorldDialog(WorldBasePath(), "main", [this](const std::string &path) {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

            worldDocument = std::make_unique<WorldDocument>(path);
            worldDocument->Save(); // write the initial world file so it exists on disk
            shell.SetWorldDocument(worldDocument.get());
            shell.SetPanelVisible("config", true);
            RefreshDocumentInfo();
            LOG_I(TAG, "new world '%s'", path.c_str());
        });
    }

    void SandboxModule::OpenWorld()
    {
        StopPlay();

        FileBrowserRequest request;
        request.mode      = FileBrowserMode::OPEN_FILE;
        request.title     = "Open World";
        request.directory = WorldBasePath();
        request.filters   = {{"World (*.world)", {"world"}}};

        shell.OpenFileBrowser(request, [this](const FileBrowserResult &result) {
            if (!result.accepted) {
                return;
            }
            auto document = std::make_unique<WorldDocument>(result.path);
            if (!document->Load()) {
                LOG_W(TAG, "failed to load world '%s'", result.path.c_str());
                return;
            }
            worldDocument = std::move(document);
            shell.SetWorldDocument(worldDocument.get());
            shell.SetPanelVisible("config", true);
            RefreshDocumentInfo();
            LOG_I(TAG, "opened world '%s'", result.path.c_str());
        });
    }

    void SandboxModule::SaveWorld()
    {
        if (worldDocument == nullptr) {
            LOG_W(TAG, "save: no world open");
            return;
        }
        if (worldDocument->Save()) {
            LOG_I(TAG, "saved world");
            RefreshDocumentInfo();
        } else {
            LOG_W(TAG, "failed to save world");
        }
    }

    void SandboxModule::CloseWorld()
    {
        if (worldDocument == nullptr) {
            LOG_W(TAG, "close: no world open");
            return;
        }
        StopPlay();
        if (worldDocument->IsDirty()) {
            worldDocument->Save();
        }
        // Detach from the shell before destroying so the config panel never sees
        // a dangling document pointer.
        shell.SetWorldDocument(nullptr);
        worldDocument.reset();
        RefreshDocumentInfo();
        LOG_I(TAG, "closed world");
    }

    void SandboxModule::Quit()
    {
        StopPlay();
        if (worldDocument != nullptr && worldDocument->IsDirty()) {
            worldDocument->Save();
        }
        if (auto *system = Interface<ISystemNotify>::Get()->GetApi()) {
            system->SetExit();
        }
    }

    void SandboxModule::StopPlay()
    {
        if (playSession.Stop()) {
            shell.SetPlayState(playSession.GetState());
            LOG_I(TAG, "PIE stopped");
        }
    }

    void SandboxModule::RefreshDocumentInfo()
    {
        const bool dirty      = worldDocument != nullptr && worldDocument->IsDirty();
        const bool docChanged = worldDocument.get() != lastDocPtr;
        if (docInfoApplied && !docChanged && dirty == lastDocDirty) {
            return;
        }
        lastDocPtr     = worldDocument.get();
        lastDocDirty   = dirty;
        docInfoApplied = true;

        const std::string name = (worldDocument != nullptr) ? std::filesystem::path(worldDocument->GetPath()).filename().string() : std::string{};
        shell.SetDocumentInfo(name, dirty);
    }

    void SandboxModule::RegisterPreferencePages()
    {
        preferenceRegistry.Clear();

        PreferencePage general;
        general.id    = "general";
        general.title = "General";
        PreferenceSection generalMain;
        generalMain.id    = "general.main";
        generalMain.title = "General";
        generalMain.entries.push_back({"general.language", "Language", PreferenceValue::Str("English"), 0.0, 0.0, {"English", "Chinese"}});
        generalMain.entries.push_back({"general.theme", "Theme", PreferenceValue::Str("Dark"), 0.0, 0.0, {"Dark", "Light"}});
        generalMain.entries.push_back({"general.autosave", "Autosave", PreferenceValue::Bool(true)});
        general.sections.push_back(std::move(generalMain));
        preferenceRegistry.RegisterPage(std::move(general));

        PreferencePage editor;
        editor.id    = "editor";
        editor.title = "Editor";
        PreferenceSection editorMain;
        editorMain.id    = "editor.main";
        editorMain.title = "Editor";
        editorMain.entries.push_back({"editor.uiScale", "UI Scale", PreferenceValue::Float(1.0), 0.5, 2.0});
        editorMain.entries.push_back({"editor.gridSize", "Grid Size", PreferenceValue::Float(10.0), 1.0, 100.0});
        editorMain.entries.push_back({"editor.snap", "Snap", PreferenceValue::Bool(true)});
        editorMain.entries.push_back({"editor.undoDepth", "Undo Depth", PreferenceValue::Int(32), 1.0, 256.0});
        editor.sections.push_back(std::move(editorMain));
        preferenceRegistry.RegisterPage(std::move(editor));

        PreferencePage rendering;
        rendering.id    = "rendering";
        rendering.title = "Rendering";
        PreferenceSection renderingMain;
        renderingMain.id    = "rendering.main";
        renderingMain.title = "Rendering";
        renderingMain.entries.push_back({"rendering.vsync", "V-Sync", PreferenceValue::Bool(true)});
        renderingMain.entries.push_back({"rendering.resolutionScale", "Resolution Scale", PreferenceValue::Float(1.0), 0.5, 2.0});
        rendering.sections.push_back(std::move(renderingMain));
        preferenceRegistry.RegisterPage(std::move(rendering));
    }

    std::string SandboxModule::PreferencePath() const
    {
        const std::string layout = LayoutPersistence::GetDefaultPath();
        const std::size_t slash  = layout.find_last_of("/\\");
        if (slash == std::string::npos) {
            return {};
        }
        return layout.substr(0, slash + 1) + "editor-preferences.json";
    }

    void SandboxModule::LoadPreferences()
    {
        const std::string path = PreferencePath();
        if (path.empty() || preferenceStore == nullptr) {
            return;
        }
        std::string json;
        if (ReadString(path, json) && !json.empty() && preferenceStore->FromJson(json)) {
            LOG_I(TAG, "loaded preferences from %s", path.c_str());
        }
    }

    void SandboxModule::SavePreferences()
    {
        const std::string path = PreferencePath();
        if (path.empty() || preferenceStore == nullptr) {
            return;
        }
        WriteString(path, preferenceStore->ToJson());
    }

    bool SandboxModule::BuildEditor()
    {
        // Mount the project workspace (writable) + engine bundle (read-only) into
        // the authoring asset DB, mirroring the legacy editor. Paths are relative
        // to each side's `assets/` root.
        if (!project.Dir().empty()) {
            auto *workFs   = new sky::NativeFileSystem(project.Dir());
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
        layoutModel.Tabify("config", "outliner"); // world config shares the Outliner dock
        layoutModel.SplitPanel("outliner", SplitOrientation::HORIZONTAL, "viewport");
        layoutModel.SplitPanel("viewport", SplitOrientation::HORIZONTAL, "inspector");
        layoutModel.SplitPanel("viewport", SplitOrientation::VERTICAL, "outputlog");
        layoutModel.Tabify("console", "outputlog");

        // SplitPanel nests by halving, which leaves the Outliner too wide. Set
        // explicit ratios for a balanced UE/Blender-like layout.
        if (auto *rootSplit = static_cast<SplitNode *>(layoutModel.GetRoot())) {
            if (rootSplit->children.size() == 2 && IsSplit(rootSplit->children[1].get())) {
                rootSplit->ratios = {0.22f, 0.78f}; // outliner | rest
                auto *inner       = static_cast<SplitNode *>(rootSplit->children[1].get());
                if (inner->children.size() == 2) {
                    inner->ratios = {0.68f, 0.32f}; // (viewport/outputlog) | inspector
                }
            }
        }

        // Snapshot the built-in default so View > Reset Layout restores it.
        layoutModel.CaptureDefault();

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

        // Register editor SVG icons as textures for the toolbar (see editor-toolbar.md).
        if (sky::ui::UITextSystem *text = renderer.GetTextSystem()) {
            if (sky::ui::IUITextureRegistry *registry = text->GetRegistry()) {
                static const char *kIcons[] = {"open", "save", "undo", "redo", "play", "pause", "stop"};
                for (const char *name : kIcons) {
                    sky::ui::UIImageData image;
                    if (RasterizeSvg(SandboxResources::Resolve(std::string("icons/") + name + ".svg"), 48u, image)) {
                        shell.SetIcon(name, registry->RegisterTexture(image));
                    }
                }
            }
        }

        shell.SetLayout(&layoutModel);
        shell.SetPanelRegistry(&panelRegistry);
        shell.SetSelection(&selection);
        static RegisteredPropertySource propertySource;
        shell.SetPropertySource(&propertySource);
        shell.SetStatusInfo(project.name, rhiName, "Edit");
        // A closed floating window re-docks its panel into the main window.
        renderer.SetSurfaceClosedCallback([this](const std::string &panelId) { shell.DockFloatingPanel(panelId, "viewport", DockPosition::CENTER); });
        renderer.SetSurfaceGeometryCallback(
            [this](const std::string &panelId, float x, float y, float w, float h) { shell.SetFloatingGeometry(panelId, x, y, w, h); });

        sky::DerivedDataCache::Get().SetRoot(SandboxResources::Resolve("cache"));
        InstallUiIconBuilder();
        shell.SetLogService(&logService);
        shell.SetCommandController(&commandController);

        RegisterPreferencePages();
        preferenceStore = std::make_unique<PreferenceStore>(&preferenceRegistry);
        LoadPreferences();
        uiScale = EffectiveUiScale();
        shell.SetUiScale(uiScale);
        shell.SetPreferences(&preferenceRegistry, preferenceStore.get(), [this]() { SavePreferences(); });
        // Play-In-Editor: the session duplicates the edit world and ticks it.
        playSession.SetWorldFactory([this]() -> sky::WorldPtr { return worldDocument != nullptr ? worldDocument->CreatePlayWorld() : nullptr; });

        // The window title reflects the open world + dirty marker (see
        // docs/editor/play-in-editor.md / editor-framework-status.md).
        shell.SetTitleHandler([](const std::string &title) {
            if (auto *window = sky::NativeWindowManager::Get()->GetMainWindow()) {
                window->SetTitle(title);
            }
        });
        RefreshDocumentInfo();

        // Editor actions: the toolbar, menus, and keyboard shortcuts all drive
        // these by id (see docs/editor/editor-toolbar.md). Plugins contribute
        // through EditorActionRegistry in their EditorExtension::Register().
        EditorActionRegistry *actions = EditorActionRegistry::Get();
        actions->Clear();
        actions->Add({.id = "file.new", .label = "New World...", .menu = "File", .order = 0, .menuOrder = 0, .invoke = [this]() { NewWorld(); }});
        actions->Add({.id        = "file.open",
                      .label     = "Open World...",
                      .icon      = "open",
                      .group     = "file",
                      .menu      = "File",
                      .order     = 1,
                      .menuOrder = 0,
                      .invoke    = [this]() { OpenWorld(); }});
        actions->Add({.id        = "file.save",
                      .label     = "Save World",
                      .icon      = "save",
                      .group     = "file",
                      .menu      = "File",
                      .order     = 2,
                      .menuOrder = 0,
                      .invoke    = [this]() { SaveWorld(); }});
        actions->Add({.id = "file.close", .label = "Close World", .menu = "File", .order = 3, .menuOrder = 0, .invoke = [this]() { CloseWorld(); }});
        actions->Add({.id = "file.quit", .label = "Quit", .menu = "File", .order = 9, .menuOrder = 0, .invoke = [this]() { Quit(); }});
        actions->Add({.id        = "edit.undo",
                      .label     = "Undo",
                      .icon      = "undo",
                      .group     = "history",
                      .menu      = "Edit",
                      .order     = 0,
                      .menuOrder = 1,
                      .enabled   = []() { return EditorCore::GetCommandService().CanUndo(); },
                      .invoke    = []() { EditorCore::GetCommandService().Undo(); }});
        actions->Add({.id        = "edit.redo",
                      .label     = "Redo",
                      .icon      = "redo",
                      .group     = "history",
                      .menu      = "Edit",
                      .order     = 1,
                      .menuOrder = 1,
                      .enabled   = []() { return EditorCore::GetCommandService().CanRedo(); },
                      .invoke    = []() { EditorCore::GetCommandService().Redo(); }});
        actions->Add(
            {.id = "view.reset", .label = "Reset Layout", .menu = "View", .order = 5, .menuOrder = 2, .invoke = [this]() { shell.ResetLayout(); }});
        actions->Add({.id = "help.demo", .label = "Demo", .menu = "Help", .order = 0, .menuOrder = 3, .invoke = [this]() {
                          shell.SetPanelVisible("refldemo", true);
                      }});
        actions->Add(
            {.id = "help.about", .label = "About", .menu = "Help", .order = 1, .menuOrder = 3, .invoke = []() { LOG_I(TAG, "SkyEngine Editor"); }});
        actions->Add({.id      = "play.play",
                      .label   = "Play",
                      .icon    = "play",
                      .group   = "play",
                      .order   = 0,
                      .enabled = [this]() { return playSession.GetState() != PlayState::Playing; },
                      .invoke =
                          [this]() {
                              if (playSession.Play()) {
                                  LOG_I(TAG, "PIE play");
                              }
                              shell.SetPlayState(playSession.GetState());
                          }});
        actions->Add({.id      = "play.pause",
                      .label   = "Pause",
                      .icon    = "pause",
                      .group   = "play",
                      .order   = 1,
                      .enabled = [this]() { return playSession.GetState() == PlayState::Playing; },
                      .invoke =
                          [this]() {
                              playSession.Pause();
                              shell.SetPlayState(playSession.GetState());
                          }});
        actions->Add({.id      = "play.stop",
                      .label   = "Stop",
                      .icon    = "stop",
                      .group   = "play",
                      .order   = 2,
                      .enabled = [this]() { return playSession.GetState() != PlayState::Editing; },
                      .invoke  = [this]() { StopPlay(); }});

        shell.RegisterBuiltinPanelViews();
        shellTarget = MakeShellTarget(&shell);
        shell.Rebuild();

        // Materialize floating windows restored from the saved layout.
        for (const FloatingPanel &fp : layoutModel.GetFloatingPanels()) {
            const uint32_t fw        = fp.width > 1.0f ? static_cast<uint32_t>(fp.width) : 720u;
            const uint32_t fh        = fp.height > 1.0f ? static_cast<uint32_t>(fp.height) : 480u;
            const uint32_t surfaceId = renderer.CreateSurface(fp.panelId, static_cast<int>(fp.x), static_cast<int>(fp.y), fw, fh);
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
        if (browser == nullptr) {
            return;
        }
        FileBrowserRequest request;
        request.mode      = FileBrowserMode::OPEN_PROJECT;
        request.title     = "Add Existing Project";
        request.filters   = {{"SkyEngine Project (*.skyproj)", {"skyproj"}}};
        request.directory = DefaultProjectsDirectory();
        browser->Open(request);
        if (hubView != nullptr) {
            hubView->SetStatus("Choose a *.skyproj to add");
        }
    }

    void SandboxModule::OnFileBrowserResult(const FileBrowserResult &result)
    {
        if (!result.accepted || hubView == nullptr) {
            return;
        }
        if (result.directory) {
            CreateProjectFromResult(result);
        } else {
            AddProjectFromResult(result);
        }
    }

    void SandboxModule::CreateProjectFromResult(const FileBrowserResult &result)
    {
        const std::filesystem::path target(result.path);
        const std::string           name   = target.filename().string();
        const std::string           parent = target.parent_path().string();
        if (name.empty() || parent.empty()) {
            hubView->SetStatus("Invalid project location");
            return;
        }
        if (std::filesystem::exists(target)) {
            hubView->SetStatus("Target already exists: " + result.path);
            return;
        }
        std::string skyproj;
        if (ProjectDescriptor::Create(parent, name, skyproj)) {
            ProjectRegistry::Get()->Add(skyproj);
            RefreshHubRecent();
            hubView->SetStatus("Created " + skyproj);
            LOG_I(TAG, "created project '%s'", skyproj.c_str());
        } else {
            hubView->SetStatus("Failed to create project under " + parent);
        }
    }

    void SandboxModule::AddProjectFromResult(const FileBrowserResult &result)
    {
        ProjectDescriptor descriptor;
        if (!descriptor.Read(result.path)) {
            hubView->SetStatus("Not a valid project: " + result.path);
            return;
        }
        std::string message;
        if (!ValidateProject(descriptor, message)) {
            hubView->SetStatus(message);
            return;
        }
        ProjectRegistry::Get()->Add(result.path);
        RefreshHubRecent();
        hubView->SetStatus(message.empty() ? ("Added " + descriptor.name) : message);
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
        const std::string dir = descriptor.Read(skyprojPath) ? descriptor.Dir() : std::filesystem::path(skyprojPath).parent_path().string();
        std::error_code   error;
        std::filesystem::remove_all(dir, error);
        ProjectRegistry::Get()->Remove(skyprojPath);
        RefreshHubRecent();
        if (hubView != nullptr) {
            hubView->SetStatus(error ? ("Failed to delete: " + dir) : ("Deleted " + dir));
        }
    }

    void SandboxModule::NewProject()
    {
        if (browser == nullptr) {
            return;
        }
        const std::string projectsDir = DefaultProjectsDirectory();
        std::error_code   error;
        std::filesystem::create_directories(projectsDir, error);

        FileBrowserRequest request;
        request.mode        = FileBrowserMode::SELECT_DIRECTORY;
        request.title       = "New Project - Choose Directory";
        request.directory   = projectsDir;
        request.defaultName = "MyProject";
        browser->Open(request);
        if (hubView != nullptr) {
            hubView->SetStatus("Choose a directory and project name");
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

        if (!hubMode) {
            // Play-In-Editor: advances the runtime world only while playing; the
            // edit world is never ticked.
            playSession.Tick(delta);
            // Reflect document edits (-> title/status dirty marker).
            RefreshDocumentInfo();
            // Refresh the toolbar items' enabled state (Undo/Redo/Play/...).
            shell.RefreshActions();
        }

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
            if (hubMode && browser != nullptr && browser->IsOpen()) {
                browser->OnPointerEvent(pointer);
            } else if (auto *target = ActiveTarget()) {
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
        key.keyCode   = ScanCodeToVirtualKey(event.scanCode);
        key.action    = sky::ui::UIKeyAction::DOWN;
        key.modifiers = static_cast<uint32_t>(event.mod);
        if (hubMode && browser != nullptr && browser->IsOpen()) {
            browser->OnKeyEvent(key);
        } else if (auto *target = ActiveTarget()) {
            target->OnKey(key);
        }
    }

    void SandboxModule::OnKeyUp(const sky::KeyboardEvent &event)
    {
        if (!AcceptWindowEvent(event.winID)) {
            return;
        }
        sky::ui::UIKeyEvent key;
        key.keyCode   = ScanCodeToVirtualKey(event.scanCode);
        key.action    = sky::ui::UIKeyAction::UP;
        key.modifiers = static_cast<uint32_t>(event.mod);
        if (hubMode && browser != nullptr && browser->IsOpen()) {
            browser->OnKeyEvent(key);
        } else if (auto *target = ActiveTarget()) {
            target->OnKey(key);
        }
    }

    void SandboxModule::OnTextInput(sky::WindowID winID, const char *text)
    {
        if (text == nullptr || !AcceptWindowEvent(winID)) {
            return;
        }
        // TranslateMessage emits WM_CHAR control codes for Backspace/Enter/Tab/Esc
        // (0x08/0x0D/0x09/0x1B); drop them so they are not inserted as text.
        sky::ui::UITextInputEvent input;
        for (const char *p = text; *p != '\0'; ++p) {
            const unsigned char c = static_cast<unsigned char>(*p);
            if (c >= 0x20 && c != 0x7F) {
                input.text.push_back(static_cast<char>(c));
            }
        }
        if (input.text.empty()) {
            return;
        }
        if (hubMode && browser != nullptr && browser->IsOpen()) {
            browser->OnTextInput(input);
        } else if (auto *target = ActiveTarget()) {
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
        browser = nullptr;
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
