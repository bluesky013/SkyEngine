//
// Created by blues on 2023/12/24.
//

#include <framework/asset/AssetManager.h>

#ifdef SKY_EDITOR
    #include <rapidjson/rapidjson.h>
#endif

#include <filesystem>
#include <fstream>

#include <core/hash/Hash.h>
#include <core/logger/Logger.h>
#include <core/file/FileIO.h>

#include <framework/asset/Asset.h>
#include <framework/platform/PlatformBase.h>

static const char* TAG = "AssetManager";
static const char* PACKAGE_PATH = "package.repo";
static const char* PRODUCTS_PATH = "products";
static const char* PRODUCTS_ASSETS_PATH = "/assets";
static const char* PRODUCTS_ASSETS_REPO_PATH = "/assets.bin";
static const char* PROJECT_ASSET_PREFIX = "/assets";
static const char* ENGINE_ASSET_PREFIX = "/engine_assets";

namespace sky {
    static constexpr Uuid EMPTY_UUID;

    static std::string MakeStandardPath(const std::string &input)
    {
        return std::filesystem::path(input).make_preferred().string();
    }

    static std::filesystem::path GetAssetPathByUUID(const std::string &workDir, const Uuid &uuid)
    {
        std::filesystem::path assetPath(workDir);
        auto strUuid = uuid.ToString();
        assetPath.append(strUuid.substr(0, 2));
        assetPath.append(strUuid + ".bin");

        return assetPath;
    }

    static std::string GetAssetLoadPath(const std::string &path, AssetGroup group = AssetGroup::PROJECT)
    {
        std::string res;
        if (group == AssetGroup::ENGINE) {
            res = ENGINE_ASSET_PREFIX;
        } else {
            res = PROJECT_ASSET_PREFIX;
        }

        res += "/" + path;
        return MakeStandardPath(res);
    }

    Uuid AssetManager::GetUUIDByPath(const std::string &path)
    {
        return Uuid::CreateWithSeed(Fnv1a32(MakeStandardPath(path)));
    }

    uint32_t AssetManager::CalculateFileHash(const std::string &loadPath)
    {
        return 0;
    }

    AssetManager::~AssetManager()
    {
#ifdef SKY_EDITOR
        if (package) {
            package->SaveToFile(projectPath + PACKAGE_PATH);
        }
#endif

        if (products) {
            products->SaveToFile(workDir + PRODUCTS_ASSETS_REPO_PATH);
        }
    }

    void AssetManager::SetWorkPath(const std::string &path)
    {
        workDir = path;
        products = std::make_unique<AssetProducts>();
        products->LoadFromFile(workDir + PRODUCTS_ASSETS_REPO_PATH);
    }

    std::string AssetManager::GetPlatformPrefix(PlatformType platform)
    {
        if (platform == PlatformType::DEFAULT) {
            platform = Platform::Get()->GetType();
        }

        switch (platform) {
        case PlatformType::WINDOWS:
            return "win32";
        case PlatformType::MACOS:
            return "macos";
        case PlatformType::IOS:
            return "ios";
        case PlatformType::ANDROID:
            return "android";
        case PlatformType::LINUX:
            return "linux";
        default:
            SKY_ASSERT(false && "invalid platform type");
            return "";
        };
    }

    std::string AssetManager::GetBuildOutputPath(const std::string &parent, PlatformType platform)
    {
        auto prefix = GetPlatformPrefix(platform);
        return prefix.empty() ? prefix : parent + PRODUCTS_PATH + "/" + prefix + PRODUCTS_ASSETS_PATH;
    }

    std::shared_ptr<AssetBase> AssetManager::CreateAsset(const Uuid &type, const Uuid &uuid)
    {
        auto hIter = assetHandlers.find(type);
        if (hIter == assetHandlers.end()) {
            return {};
        }
        auto asset = hIter->second->CreateAsset();
        asset->SetUuid(uuid);

        {
            std::lock_guard<std::mutex> lock(assetMutex);
            assetMap.emplace(uuid, asset);
        }

        return asset;
    }

    std::string AssetManager::GetAssetPath(const Uuid &uuid)
    {
        return GetAssetPathByUUID(workDir, uuid).string();
    }

    std::shared_ptr<AssetBase> AssetManager::LoadAsset(const Uuid &type, const std::string &path, bool async)
    {
        auto id = GetUUIDByPath(GetAssetLoadPath(path));
        if (!products->HasAsset(id)) {
            id = GetUUIDByPath(GetAssetLoadPath(path, AssetGroup::ENGINE));
        }
        return LoadAsset(type, id, async);
    }

    std::shared_ptr<AssetBase> AssetManager::LoadAsset(const Uuid &type, const Uuid &uuid, bool async)
    {
        std::shared_ptr<AssetBase> asset;
        // check asset loaded.
        {
            std::lock_guard<std::mutex> lock(assetMutex);

            auto iter = assetMap.find(uuid);
            if (iter != assetMap.end()) {
                std::shared_ptr<AssetBase> res = iter->second.lock();
                if (res) {
                    return res;
                }
            }
        }

        // load asset from work path
        const auto *info = products->GetProduct(uuid);
        if (info == nullptr) {
            // try build asset if exists
            LOG_E(TAG, "Load Asset Failed %s", uuid.ToString().c_str());
            return {};
        }

        auto hIter = assetHandlers.find(type);
        if (hIter == assetHandlers.end()) {
            return {};
        }
        auto *handler = hIter->second.get();

        asset = CreateAsset(type, uuid);
        asset->status = AssetBase::Status::LOADING;

        std::string loadPath = GetAssetPathByUUID(workDir, uuid).string();
        auto fn = [handler, asset, loadPath]() {
            handler->LoadFromPath(loadPath, asset);
            asset->status = AssetBase::Status::LOADED;
        };

        if (async) {
            tf::Taskflow flow;
            flow.emplace(std::move(fn));
            asset->future = JobSystem::Get()->Run(std::move(flow));
        } else {
            fn();
        }
        return asset;
    }

    void AssetManager::RegisterAssetHandler(const Uuid &type, AssetHandlerBase *handler)
    {
        assetHandlers[type].reset(handler);
    }

    AssetHandlerBase*AssetManager::GetAssetHandler(const Uuid &type)
    {
        auto iter = assetHandlers.find(type);
        if (iter == assetHandlers.end()) {
            return nullptr;
        }
        return iter->second.get();
    }

#ifdef SKY_EDITOR
    void AssetManager::LoadConfig(const std::string &path)
    {
        std::string json;
        if (!ReadString(path, json)) {
            LOG_W(TAG, "Load Config Failed");
            return;
        }

        rapidjson::Document document;
        document.Parse(json.c_str());

        std::string parent = std::filesystem::path(path).parent_path().string();

        for (auto iter = document.MemberBegin(); iter != document.MemberEnd(); iter++) {
            const char* key = iter->name.GetString();
            const char* cfgPath = iter->value.GetString();
            for (auto &builder : assetBuilders) {
                if (builder->GetConfigKey() == std::string(key)) {
                    builder->LoadConfig(parent + "/" + cfgPath);
                }
            }
        }
    }

    void AssetManager::SetProjectPath(const std::string &path)
    {
        projectPath = path + "/";
        projectAssetPath = path + PROJECT_ASSET_PREFIX;
        engineAssetPath = path + ENGINE_ASSET_PREFIX;

        searchPathList.emplace_back(AssetSearchPath{projectPath});
        searchPathList.emplace_back(AssetSearchPath{engineAssetPath, AssetGroup::ENGINE});
        searchPathList.emplace_back(AssetSearchPath{projectAssetPath, AssetGroup::PROJECT});

        package = std::make_unique<AssetPackage>();
        package->LoadFromFile(projectPath + PACKAGE_PATH);

        LoadConfig(projectPath + "config/asset.json");

        SetWorkPath(GetBuildOutputPath(projectPath, PlatformType::DEFAULT));
    }

    const Uuid &AssetManager::RegisterAsset(const SourceAssetInfo &info)
    {
        return package->RegisterAsset(GetUUIDByPath(info.loadPath), info);
    }

    const Uuid &AssetManager::ImportAndBuildAsset(const std::string &path, PlatformType target)
    {
        const auto &id = ImportAsset(path);
        return BuildAsset(id, target) ? id : EMPTY_UUID;
    }

    const Uuid &AssetManager::ImportAsset(const std::string &path)
    {
        SourceAssetInfo info = {};
        for (const auto &tmpPath : searchPathList) {
            std::filesystem::path assetPath(tmpPath.path);
            assetPath.append(path);

            if (std::filesystem::exists(assetPath)) {
                info.loadPath = GetAssetLoadPath(path, tmpPath.group);
                info.hash = CalculateFileHash(assetPath.string());
                break;
            }
        }
        if (info.loadPath.empty()) {
            LOG_E(TAG, "import asset failed %s", path.c_str());
            return EMPTY_UUID;
        }
        return RegisterAsset(info);
    }

    void AssetManager::RemoveAsset(const Uuid &uuid)
    {
        package->RemoveAsset(uuid);
    }

    bool AssetManager::BuildAsset(const BuildRequest &request)
    {
        auto iter = assetBuilderMap.find(request.ext);
        if (iter == assetBuilderMap.end()) {
            LOG_E(TAG, "Asset Builder not Found %s", request.fullPath.c_str());
            return false;
        }
        BuildResult result = {};

        auto &builders = iter->second;
        for (auto &builder : builders) {
            builder->Request(request, result);
        }

        if (!result.success) {
            LOG_E(TAG, "Build Asset Failed. %s", request.name.c_str());
            return false;
        }
        for (auto &product : result.products) {
            products->RegisterDependencies(product.asset->GetUuid(), product.deps);
            SaveAsset(product.asset, request.targetPlatform);
        }

        return true;
    }

    bool AssetManager::BuildAsset(const Uuid &uuid, PlatformType target)
    {
        const auto *sourceInfo = package->GetAssetInfo(uuid);
        if (sourceInfo == nullptr) {
            LOG_E(TAG, "asset not imported. %s", uuid.ToString().c_str());
            return false;
        }

        std::filesystem::path fs(sourceInfo->loadPath);
        auto ext = fs.extension().string();

        BuildRequest request = {};
        request.relativePath = sourceInfo->loadPath;
        request.fullPath = projectPath + sourceInfo->loadPath;
        request.ext = ext;
        request.name = fs.filename().string();
        request.uuid = uuid;
        request.targetPlatform = target;

        BuildResult result = {};
        auto iter = assetBuilderMap.find(ext);
        if (iter == assetBuilderMap.end()) {
            LOG_E(TAG, "Asset Build not Found %s", sourceInfo->loadPath.c_str());
            return false;
        }
        auto &builders = iter->second;
        for (auto &builder : builders) {
            builder->Request(request, result);
        }

        if (!result.success) {
            LOG_E(TAG, "Build Asset Failed. %s", sourceInfo->loadPath.c_str());
            return false;
        }
        for (auto &product : result.products) {
            products->RegisterDependencies(product.asset->GetUuid(), product.deps);
            SaveAsset(product.asset, target);
        }

        return true;
    }

    void AssetManager::SaveAsset(const Uuid &uuid, PlatformType target)
    {
        SaveAsset(assetMap[uuid].lock(), target);
    }

    void AssetManager::SaveAsset(const std::shared_ptr<AssetBase> &asset, PlatformType target)
    {
        auto hIter = assetHandlers.find(asset->GetType());
        if (hIter == assetHandlers.end()) {
            return;
        }

        auto outputPath = GetBuildOutputPath(projectPath, target);
        if (outputPath.empty()) {
            return;
        }
        std::filesystem::path fOut = GetAssetPathByUUID(outputPath, asset->GetUuid());
        if (!std::filesystem::exists(fOut.parent_path())) {
            std::filesystem::create_directories(fOut.parent_path());
        }

        auto *assetHandler = hIter->second.get();
        assetHandler->SaveToPath(fOut.string(), asset);

        ProductAssetInfo info = {};
        products->AddAsset(asset->uuid, info);
    }

    void AssetManager::RegisterBuilder(AssetBuilder *builder)
    {
        assetBuilders.emplace_back(builder);
        const auto &extensions = builder->GetExtensions();
        for (const auto &ext : extensions) {
            assetBuilderMap[ext].emplace_back(assetBuilders.back().get());
        }
    }
#endif
} // namespace sky