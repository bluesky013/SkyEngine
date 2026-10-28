//
// Created by Zach Lee on 2022/9/25.
//

#include <framework/application/GameApplication.h>

#include <core/cmdline/CmdParser.h>
#include <string>

#include <core/file/FileIO.h>
#include <core/file/FileSystem.h>
#include <core/file/MultiFileSystem.h>
#include <core/logger/Logger.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <filesystem>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/platform/PlatformBase.h>
#include <framework/world/IGameWorld.h>
#include <framework/world/WorldAsset.h>

#include <vector>

static const char *TAG         = "Application";
static const char *CONFIG_PATH = "configs/modules_game.json";

namespace sky {

    FilePtr OpenGameConfig(const FileSystemPtr &workFs, const FileSystemPtr &bundleFs)
    {
        MultiFileSystem configs;
        if (workFs != nullptr) {
            configs.AddFileSystem(workFs);
        }
        if (bundleFs != nullptr) {
            configs.AddFileSystem(bundleFs);
        }
        return configs.OpenFile(CONFIG_PATH);
    }

    GameApplication::GameApplication() = default;

    GameApplication::~GameApplication()
    {
        Shutdown();
    }

    void GameApplication::OnShutdown()
    {
        ReleaseWorld();
        auto *service = Interface<IGameWorld>::Get();
        if (service->GetApi() == this) {
            service->UnRegister();
        }
    }

    namespace {
        // Product bundles are packaged under `products/<bundle>/` with a
        // `products/bundles.json` manifest (see the add-packaged-asset-bundles
        // change). Mount exactly the listed bundles; fall back to `common` when the
        // manifest is absent (e.g. a dev/source run).
        void MountProductBundles(const FileSystemPtr &root)
        {
            std::vector<std::string> bundles;
            if (FilePtr manifest = root->OpenFile("products/bundles.json")) {
                std::string text;
                if (manifest->ReadString(text)) {
                    rapidjson::Document document;
                    document.Parse(text.c_str());
                    if (!document.HasParseError() && document.HasMember("bundles") && document["bundles"].IsArray()) {
                        for (const auto &entry : document["bundles"].GetArray()) {
                            if (entry.IsString()) {
                                bundles.emplace_back(entry.GetString());
                            }
                        }
                    }
                }
            }
            if (bundles.empty()) {
                bundles.emplace_back("common");
            }
            for (const auto &key : bundles) {
                auto bundleFs = root->CreateSubSystem("products/" + key, false);
                AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(bundleFs, key));
                LOG_I(TAG, "mounted product bundle '%s'", key.c_str());
            }
        }

        bool HasProjectArg(int argc, char **argv)
        {
            for (int i = 1; i < argc; ++i) {
                if (std::string(argv[i]) == "--project") {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    bool GameApplication::Init(int argc, char **argv)
    {
        // Product bundles are a packaged-build concern; `--project` dev mode mounts
        // the project tree (source assets) later in ParseStartArgs.
        const bool mountBundles = !HasProjectArg(argc, argv);
#if defined(SKY_PLATFORM_ANDROID) || defined(SKY_PLATFORM_IOS)
        // Mobile work files stay in the writable app data directory; packaged
        // configs/products are read separately through the bundle filesystem.
        workFs = new NativeFileSystem(Platform::Get()->GetInternalPath());
        AssetManager::Get()->SetWorkFileSystem(workFs);
        if (mountBundles) {
            MountProductBundles(Platform::Get()->GetBundleFileSystem());
        }
#else
        // No project argument outside editor mode: use the bundle path so the
        // builtin configs/assets deployed next to the executable are found.
        workFs = new NativeFileSystem(Platform::Get()->GetBundlePath());
        AssetManager::Get()->SetWorkFileSystem(workFs);
        if (mountBundles) {
            MountProductBundles(workFs);
        }
#endif
        if (!Application::Init(argc, argv)) {
            return false;
        }
        if (!worldReady) {
            Shutdown();
            return false;
        }
        return true;
    }

    void GameApplication::ParseStartArgs()
    {
        // `--project <path>`: mount the project directory as the work file system
        // (so its configs/ and assets/ resolve) and remember its descriptor.
        const auto &values = arguments.values;
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (*it != "--project") {
                continue;
            }
            auto next = std::next(it);
            if (next == values.end()) {
                LOG_E(TAG, "--project requires a path argument");
                break;
            }
            // Accept either the .skyproj file or the project directory.
            std::string     path = *next;
            std::error_code error;
            if (std::filesystem::is_directory(path, error)) {
                for (const auto &entry : std::filesystem::directory_iterator(path, error)) {
                    if (entry.path().extension() == ".skyproj") {
                        path = entry.path().string();
                        break;
                    }
                }
            }
            if (project.Read(path)) {
                hasProject = true;
                workFs     = new NativeFileSystem(project.Dir());
                AssetManager::Get()->SetWorkFileSystem(workFs);
                LOG_I(TAG, "Project mounted: %s (%s)", project.name.c_str(), project.Dir().c_str());
            } else {
                LOG_E(TAG, "Failed to read project: %s", path.c_str());
            }
            break;
        }

        // Apply the project's preferred RHI backend unless the CLI overrides it
        // (`-r`/`--rhi`). AuroraModule parses this from the module arguments.
        if (hasProject && !project.rhi.empty()) {
            bool overridden = false;
            for (const auto &value : arguments.values) {
                if (value == "--rhi" || value == "-r") {
                    overridden = true;
                    break;
                }
            }
            if (!overridden) {
                arguments.values.emplace_back("--rhi");
                arguments.args.emplace_back(arguments.values.back().c_str());
                arguments.values.emplace_back(project.rhi);
                arguments.args.emplace_back(arguments.values.back().c_str());
            }
        }
    }

    void GameApplication::PreTick()
    {
    }

    void GameApplication::Tick(float delta)
    {
        if (worldReady && worldStarted && world) {
            world->Tick(delta);
        }
    }

    World *GameApplication::GetWorld() const
    {
        return worldReady ? world.Get() : nullptr;
    }

    void GameApplication::ReleaseWorld()
    {
        worldReady          = false;
        const auto released = world;
        world               = nullptr;
        const bool stop     = worldStarted;
        worldStarted        = false;
        if (released && stop) {
            released->StopSimulation();
        }
    }

    bool GameApplication::LoadDefaultWorld()
    {
        if (defaultScene.empty()) {
            return true;
        }
        if (hasProject) {
            MountProductBundles(workFs);
        }
        const auto product = AssetManager::Get()->LoadAssetFromPath<World>(defaultScene, AssetLoadPolicy::ProductsOnly);
        if (!product) {
            LOG_E(TAG, "Default World product is missing or has the wrong type: %s", defaultScene.c_str());
            return false;
        }
        product->BlockUntilLoaded();
        if (!product->IsLoaded() || !product->Data().IsLoadValid()) {
            LOG_E(TAG, "Default World binary product is invalid: %s (%s)", defaultScene.c_str(), product->Data().GetLoadError().c_str());
            return false;
        }
        std::string error;
        if (!InstantiateWorldAsset(product->Data(), world, error)) {
            LOG_E(TAG, "Default World failed to load: %s (%s)", defaultScene.c_str(), error.c_str());
            return false;
        }
        LOG_I(TAG, "Default World ready: %s (%zu actors)", defaultScene.c_str(), world->GetActors().size());
        return true;
    }

    bool GameApplication::LoadConfigs()
    {
        std::unordered_map<std::string, ModuleInfo> modules = {};
        modules.emplace("Aurora.Render", ModuleInfo{"Aurora.Render", {"Aurora.Metadata"}});
        for (auto &[key, info] : modules) {
            moduleManager->RegisterModule(info);
        }

        // Project modules come from the descriptor, so a project still loads its
        // module even without a configs/modules_game.json.
        if (hasProject) {
            for (const auto &id : project.modules) {
                moduleManager->RegisterModule(ModuleInfo{id, {}});
            }
        }

        std::string   json;
        const FilePtr file = OpenGameConfig(workFs, Platform::Get()->GetBundleFileSystem());
        if (!file || !file->ReadString(json)) {
            LOG_E(TAG, "Load Config Failed: %s", CONFIG_PATH);
            return false;
        }

        rapidjson::Document document;
        document.Parse(json.c_str());
        if (document.HasParseError()) {
            LOG_E(TAG, "Parse Config Failed: %s (%u)", rapidjson::GetParseError_En(document.GetParseError()),
                  static_cast<uint32_t>(document.GetErrorOffset()));
            return false;
        }

        if (!document.IsObject()) {
            LOG_E(TAG, "Config Root Is Not An Object: %s", CONFIG_PATH);
            return false;
        }

        if (document.HasMember("modules")) {
            if (!document["modules"].IsArray()) {
                LOG_E(TAG, "Config 'modules' Is Not An Array: %s", CONFIG_PATH);
                return false;
            }
            auto array = document["modules"].GetArray();
            for (auto &module : array) {
                if (!module.IsObject()) {
                    LOG_E(TAG, "Invalid Module Entry In Config: %s", CONFIG_PATH);
                    return false;
                }

                if (!module.HasMember("name") || !module["name"].IsString()) {
                    continue;
                }

                if (module.HasMember("buildOption")) {
                    if (!module["buildOption"].IsString()) {
                        LOG_E(TAG, "Module buildOption must be a known build option string");
                        return false;
                    }
                    const auto enabled = ModuleManager::GetBuildOption(module["buildOption"].GetString());
                    if (!enabled) {
                        LOG_E(TAG, "Unknown module buildOption: %s", module["buildOption"].GetString());
                        return false;
                    }
                    if (!*enabled)
                        continue;
                }

                ModuleInfo info;
                info.name = module["name"].GetString();

                if (module.HasMember("dependencies")) {
                    if (!module["dependencies"].IsArray()) {
                        LOG_E(TAG, "Module Dependencies Is Not An Array: %s", info.name.c_str());
                        return false;
                    }
                    auto depArray = module["dependencies"].GetArray();
                    for (auto &dep : depArray) {
                        if (!dep.IsString()) {
                            LOG_E(TAG, "Module Dependency Is Not A String: %s", info.name.c_str());
                            return false;
                        }
                        info.dependencies.emplace_back(dep.GetString());
                    }
                }
                moduleManager->RegisterModule(info);
            }
        }

        if (document.HasMember("game")) {
            if (!document["game"].IsObject()) {
                LOG_E(TAG, "Config 'game' Is Not An Object: %s", CONFIG_PATH);
                return false;
            }
            auto obj = document["game"].GetObject();
            if (obj.HasMember("defaultScene")) {
                if (!obj["defaultScene"].IsString()) {
                    LOG_E(TAG, "Config 'game.defaultScene' must be a string");
                    return false;
                }
                defaultScene = obj["defaultScene"].GetString();
            }
            if (obj.HasMember("width")) {
                if (!obj["width"].IsUint() || obj["width"].GetUint() == 0) {
                    LOG_E(TAG, "Config 'game.width' Is Invalid: %s", CONFIG_PATH);
                    return false;
                }
                width = obj["width"].GetUint();
            }
            if (obj.HasMember("height")) {
                if (!obj["height"].IsUint() || obj["height"].GetUint() == 0) {
                    LOG_E(TAG, "Config 'game.height' Is Invalid: %s", CONFIG_PATH);
                    return false;
                }
                height = obj["height"].GetUint();
            }
        }

        if (hasProject) {
            defaultScene = project.defaultScene;
        }

        return true;
    }

    bool GameApplication::PreInit()
    {
        auto *handle = Platform::Get()->GetMainWinHandle();
        nativeWindow.reset(NativeWindow::Create(NativeWindow::Descriptor{width, height, "SkyGame", "SkyGame", handle}));
        if (nativeWindow == nullptr) {
            LOG_E(TAG, "Create Native Window Failed");
            return false;
        }
        auto *service = Interface<IGameWorld>::Get();
        if (service->GetApi() != nullptr) {
            LOG_E(TAG, "An application World provider is already active");
            return false;
        }
        service->Register(*this);
        return true;
    }

    bool GameApplication::InitStartupData()
    {
        worldReady = LoadDefaultWorld();
        return worldReady;
    }

    void GameApplication::PostInit()
    {
        if (worldReady && world) {
            worldStarted      = true;
            const auto active = world;
            active->StartSimulation();
        }
    }

    void *GameApplication::GetMainWindowHandle() const
    {
        return nativeWindow != nullptr ? nativeWindow->GetNativeHandle() : nullptr;
    }
} // namespace sky
