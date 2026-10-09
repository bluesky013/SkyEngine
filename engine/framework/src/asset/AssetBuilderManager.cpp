//
// Created by blues on 2024/6/21.
//

#include <core/file/FileUtil.h>
#include <core/platform/Platform.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/InProcessCookRunner.h>
#include <framework/asset/OutOfProcessCookRunner.h>
#include <framework/platform/PlatformBase.h>

namespace sky {

    namespace {

        std::string DefaultWorkerPath()
        {
            std::string dir = Platform::Get()->GetBundlePath();
            if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
                dir += '/';
            }
#if SKY_PLATFORM_WINDOWS
            return dir + "AssetTool.exe";
#else
            return dir + "AssetTool";
#endif
        }
    } // namespace

    AssetBuilderManager::~AssetBuilderManager()
    {
        // The runner is destroyed after this body; drop the completion first so its Drain
        // cannot call back into AssetManager during (unordered) singleton teardown.
        if (cookRunner) {
            cookRunner->SetCompletion({});
        }
    }

    AssetBuilder *AssetBuilderManager::QueryBuilder(const std::string &ext) const
    {
        auto iter = assetBuilderMap.find(ext);
        return iter == assetBuilderMap.end() ? nullptr : iter->second;
    }

    std::vector<std::string> AssetBuilderManager::GetExtensions() const
    {
        std::vector<std::string> extensions;
        extensions.reserve(assetBuilderMap.size());
        for (const auto &[ext, builder] : assetBuilderMap) {
            extensions.push_back(ext);
        }
        return extensions;
    }

    bool AssetBuilderManager::HasBuilder(const std::string &ext) const
    {
        return QueryBuilder(ext) != nullptr;
    }

    std::vector<std::pair<std::string, std::string>> AssetBuilderManager::GetBuilderSettings(const std::string      &ext,
                                                                                             const ProductBundleKey &bundle) const
    {
        auto *builder = QueryBuilder(ext);
        return builder != nullptr ? builder->DescribeSettings(bundle) : std::vector<std::pair<std::string, std::string>>{};
    }

    std::vector<std::pair<std::string, std::string>>
    AssetBuilderManager::GetBuilderSettings(const std::string &ext, const ProductBundleKey &bundle, const BuildSettingsOverride &override) const
    {
        auto *builder = QueryBuilder(ext);
        return builder != nullptr ? builder->DescribeSettings(bundle, override) : std::vector<std::pair<std::string, std::string>>{};
    }

    const TypeInfoRT *AssetBuilderManager::GetBuilderSettingsType(const std::string &ext) const
    {
        auto *builder = QueryBuilder(ext);
        return builder != nullptr ? builder->GetSettingsType() : nullptr;
    }

    Any AssetBuilderManager::MakeBuilderSettings(const std::string &ext, const ProductBundleKey &bundle, const BuildSettingsOverride &override) const
    {
        auto *builder = QueryBuilder(ext);
        return builder != nullptr ? builder->MakeSettings(bundle, override) : Any{};
    }

    BuildSettingsOverride AssetBuilderManager::DiffBuilderSettings(const std::string &ext, const ProductBundleKey &bundle, const Any &edited) const
    {
        auto *builder = QueryBuilder(ext);
        return builder != nullptr ? builder->DiffSettings(bundle, edited) : BuildSettingsOverride{};
    }
    void AssetBuilderManager::SetEngineFs(const NativeFileSystemPtr &fs)
    {
        engineFs = fs;
    }

    void AssetBuilderManager::SetInterMediateFs(const NativeFileSystemPtr &fs)
    {
        intermediateFs = fs;
    }

    void AssetBuilderManager::SetWorkSpaceFs(const NativeFileSystemPtr &fs)
    {
        workSpaceFs = fs;

        // create products directory
        auto productFs = workSpaceFs->CreateSubSystem("products", true);
        auto configFs  = workSpaceFs->CreateSubSystem("configs", false);

        // Load per-kind build presets (e.g. image_build_presets.json) into the registered builders, so
        // both in-process cooking and the editor's reflected settings operate on the same presets.
        if (configFs != nullptr) {
            LoadBuildConfigs(configFs);
        }

        auto *am = AssetManager::Get();
        // Unified cook/build config; falls back to the legacy presets file name.
        auto file = configFs->OpenFile("asset_cook.jsonc");
        if (!file) {
            file = configFs->OpenFile("asset_build_presets.json");
        }
        if (file) {
            std::string text;
            file->ReadString(text);

            CookConfig parsed;
            parsed.Parse(text);
            SetCookConfig(parsed);

            auto bundles = parsed.GetBundles();
            if (bundles.empty()) {
                bundles.emplace_back("common");
            }
            for (auto &bundle : bundles) {
                auto bundleFs = productFs->CreateSubSystem(bundle, true);
                am->AddAssetProductBundle(new HashedAssetBundle(bundleFs, bundle));
            }
        }

        // Select the cook backend from the parsed config (D5); default in-process keeps the
        // built-in inline path (cookRunner == null on AssetManager).
        const CookConfig effective = GetCookConfig();
        outOfProcessActive         = !forceInProcess && effective.GetMode() == CookMode::OutOfProcess;
        if (outOfProcessActive) {
            CookRunnerConfig runnerConfig;
            runnerConfig.workerPath  = effective.GetWorkerPath().empty() ? DefaultWorkerPath() : effective.GetWorkerPath();
            runnerConfig.projectPath = workSpaceFs->GetPath().GetStr();
            if (engineFs) {
                runnerConfig.enginePath = engineFs->GetPath().GetStr();
            }
            if (intermediateFs) {
                runnerConfig.intermediatePath = intermediateFs->GetPath().GetStr();
            }
            runnerConfig.platform  = effective.GetActivePlatform();
            runnerConfig.timeoutMs = effective.GetWorkerTimeoutMs();

            cookRunner = std::make_unique<OutOfProcessCookRunner>(std::move(runnerConfig));
            am->SetCookRunner(cookRunner.get());
        } else {
            cookRunner.reset();
            am->SetCookRunner(nullptr);
        }
    }

    void AssetBuilderManager::RegisterBuilder(AssetBuilder *builder)
    {
        assetBuilders.emplace_back(builder);
        const auto &extensions = builder->GetExtensions();
        for (const auto &ext : extensions) {
            assetBuilderMap[ext] = assetBuilders.back().get();
        }
    }

    void AssetBuilderManager::LoadBuildConfigs(const FileSystemPtr &fs)
    {
        for (auto &builder : assetBuilders) {
            builder->LoadConfig(fs);
        }
    }

    void AssetBuilderManager::UnRegisterBuilder(AssetBuilder *builder)
    {
        {
            auto iter = std::find_if(assetBuilders.begin(), assetBuilders.end(), [builder](const auto &val) { return builder == val.get(); });
            if (iter != assetBuilders.end()) {
                assetBuilders.erase(iter);
            }
        }

        {
            for (auto iter = assetBuilderMap.begin(); iter != assetBuilderMap.end();) {
                if (iter->second == builder) {
                    iter = assetBuilderMap.erase(iter);
                } else {
                    ++iter;
                }
            }
        }
    }

    namespace {

        // Raises the build-finished event and invokes the caller's completion. The single place a cook
        // result is published, so every path (success, failure, missing asset/source/builder) notifies
        // observers (editor cook state) and completes the on-demand load.
        void EmitBuildResult(const AssetBuildResult &result, const AssetBuilderManager::BuildCompletion &onFinished)
        {
            AsseEvent::BroadCast(result.uuid, &IAssetEvent::OnAssetBuildFinished, result);
            if (onFinished) {
                onFinished(result);
            }
        }

        AssetBuildResult MakeBuildFailure(const Uuid &uuid, const std::string &target, const std::string &error)
        {
            AssetBuildResult result = {};
            result.uuid             = uuid;
            result.target           = target;
            result.retCode          = AssetBuildRetCode::FAILED;
            result.error            = error;
            return result;
        }

    } // namespace

    void AssetBuilderManager::BuildRequest(const Uuid &uuid, const std::string &target, BuildCompletion onFinished)
    {
        auto srcAsset = AssetDataBase::Get()->FindAsset(uuid);
        if (srcAsset == nullptr) {
            EmitBuildResult(MakeBuildFailure(uuid, target, "asset not found"), onFinished);
            return;
        }

        AssetBuildRequest request = {};
        request.assetInfo         = srcAsset;
        request.file              = AssetDataBase::Get()->OpenFile(srcAsset);
        request.target            = target;
        request.bundle            = GetCookConfig().ResolveBundleForTarget(target);
        request.settings          = GetCookConfig().GetTargetSettings(AssetDataBase::Get()->GetCookJson(uuid), target);

        if (request.file == nullptr) {
            EmitBuildResult(MakeBuildFailure(uuid, target, "source file missing"), onFinished);
            return;
        }
        BuildRequest(request, std::move(onFinished));
    }

    void AssetBuilderManager::BuildRequest(const AssetBuildRequest &request, BuildCompletion onFinished)
    {
        // Single-writer invariant (D11): when out-of-process cook is active the worker is
        // the only writer of product bundles, so the editor must not cook in-process.
        SKY_ASSERT(!outOfProcessActive && "in-process cook requested while out-of-process cook is active");

        const auto key = request.assetInfo->uuid.ToString() + "#" + request.target;
        AssetExecutor::Get()->PushSavingTask(key, [this, request, onFinished = std::move(onFinished)]() {
            auto *builder = QueryBuilder(request.assetInfo->ext);
            request.assetInfo->dependencies.clear();

            AssetBuildResult result = {};
            result.uuid             = request.assetInfo->uuid;
            result.target           = request.target;
            if (builder == nullptr) {
                result = MakeBuildFailure(request.assetInfo->uuid, request.target, "no builder for extension '" + request.assetInfo->ext + "'");
            } else {
                builder->Request(request, result);
            }

            EmitBuildResult(result, onFinished);
        });
    }

    void AssetBuilderManager::BuildRequestSync(const Uuid &uuid, const std::string &target)
    {
        auto srcAsset = AssetDataBase::Get()->FindAsset(uuid);
        if (!srcAsset) {
            return;
        }

        auto *builder = QueryBuilder(srcAsset->ext);
        if (builder == nullptr) {
            return;
        }

        AssetBuildRequest request = {};
        request.assetInfo         = srcAsset;
        request.file              = AssetDataBase::Get()->OpenFile(srcAsset);
        if (request.file == nullptr) {
            return;
        }
        request.target   = target;
        request.bundle   = GetCookConfig().ResolveBundleForTarget(target);
        request.settings = GetCookConfig().GetTargetSettings(AssetDataBase::Get()->GetCookJson(uuid), target);

        srcAsset->dependencies.clear();

        AssetBuildResult result = {};
        result.uuid             = uuid;
        result.target           = target;
        builder->Request(request, result);

        // Raise the same completion event as the async path so observers (editor cook state) update,
        // even when the sync path is driven by the inline on-demand cook.
        EmitBuildResult(result, {});
    }

    void AssetBuilderManager::RequestCook(const Uuid &uuid, const std::string &target, BuildCompletion onFinished)
    {
        if (outOfProcessActive && cookRunner != nullptr) {
            std::string path;
            if (auto src = AssetDataBase::Get()->FindAsset(uuid); src != nullptr) {
                path = src->path.GetStr();
            }
            // Completion goes to AssetManager (its single SetCompletion); observers still get the event.
            cookRunner->Request(CookJob{uuid, target, path});
            return;
        }
        BuildRequest(uuid, target, std::move(onFinished));
    }

    Any AssetBuilderManager::GetImportConfig(const FilePath &filePath)
    {
        auto  ext     = filePath.Extension();
        auto *builder = QueryBuilder(ext);
        if (builder == nullptr) {
            return Any{};
        }
        return builder->RequireImportSetting(filePath);
    }

    void AssetBuilderManager::ImportAsset(const AssetImportRequest &request)
    {
        auto  ext     = request.filePath.Extension();
        auto *builder = QueryBuilder(ext);
        if (builder == nullptr) {
            return;
        }

        AssetExecutor::Get()->DependentAsync([request, builder]() { builder->Import(request); });
    }
} // namespace sky
