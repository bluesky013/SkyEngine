//
// Created by blues on 2024/6/21.
//

#include <framework/asset/AssetBuilderManager.h>

#include "AssetBuildGuard.h"
#include <core/file/FileUtil.h>
#include <core/file/MultiFileSystem.h>
#include <core/platform/Platform.h>
#include <core/thread/ThreadDispatcher.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/InProcessCookRunner.h>
#include <framework/asset/OutOfProcessCookRunner.h>
#include <framework/platform/PlatformBase.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    namespace {

        AssetSourcePtr MakeBuildSource(const AssetSourcePtr &source)
        {
            AssetSourcePtr copy = new AssetSourceInfo;
            copy->uuid          = source->uuid;
            copy->revision      = source->revision;
            copy->path          = source->path;
            copy->name          = source->name;
            copy->ext           = source->ext;
            copy->mount         = source->mount;
            return copy;
        }

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

    bool AssetBuilderManager::SetWorkSpaceFs(const NativeFileSystemPtr &fs)
    {
        if (!fs)
            return false;
        const auto    productFs      = fs->CreateSubSystem("products", true);
        const auto    projectConfigs = fs->CreateSubSystem("configs", false);
        const auto    engineConfigs  = engineFs ? engineFs->CreateSubSystem("configs", false) : FileSystemPtr{};
        auto          configs        = new MultiFileSystem();
        FileSystemPtr configFs       = configs;
        if (projectConfigs)
            configs->AddFileSystem(projectConfigs);
        if (engineConfigs)
            configs->AddFileSystem(engineConfigs);
        // Project legacy presets also override engine defaults using the newer filename.
        const auto findCookConfig = [](const FileSystemPtr &fs) -> FilePtr {
            if (!fs)
                return {};
            auto file = fs->OpenFile("asset_cook.jsonc");
            return file ? file : fs->OpenFile("asset_build_presets.json");
        };
        auto file = findCookConfig(projectConfigs);
        if (!file)
            file = findCookConfig(engineConfigs);
        CookConfig parsed;
        if (file) {
            std::string text;
            if (!file->ReadString(text) || !parsed.Parse(text))
                return false;
        }
        workSpaceFs = fs;
        LoadBuildConfigs(configFs);
        SetCookConfig(parsed);

        auto *am      = AssetManager::Get();
        auto  bundles = parsed.GetBundles();
        if (bundles.empty())
            bundles.emplace_back("common");
        for (const auto &bundle : bundles) {
            const auto bundleFs = productFs->CreateSubSystem(bundle, true);
            am->AddAssetProductBundle(new HashedAssetBundle(bundleFs, bundle));
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
        return true;
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
        AssetExecutor::Get()->WaitForAll();
        std::lock_guard lock(*configMutex);
        configRevision->fetch_add(1, std::memory_order_acq_rel);
        configInputs = {FilePath("asset_cook.jsonc"), FilePath("asset_build_presets.json")};
        for (const auto &builder : assetBuilders)
            for (const auto &file : builder->ConfigurationFiles())
                if (std::find(configInputs.begin(), configInputs.end(), file) == configInputs.end())
                    configInputs.push_back(file);
        std::sort(configInputs.begin(), configInputs.end(), [](const auto &a, const auto &b) { return a.GetStr() < b.GetStr(); });
        const std::vector<NativeFileSystemPtr> roots{engineFs, workSpaceFs};
        std::string                            before, after;
        const auto                             captured = CaptureCookConfiguration(roots, configInputs, before);
        for (auto &builder : assetBuilders) {
            builder->LoadConfig(fs);
        }
        const auto checked      = CaptureCookConfiguration(roots, configInputs, after);
        loadedConfigFilesValid  = captured && checked && before == after;
        loadedConfigFingerprint = std::move(after);
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
            // The broadcast may originate on a cook worker or the caller thread; deliver it on the
            // main role so observers (editor cook state) never run off the main thread. `onFinished`
            // stays direct: it resolves on-demand loads, some of which block the main thread.
            ThreadDispatcher::Get()->PostToMain([result]() { AsseEvent::BroadCast(result.uuid, &IAssetEvent::OnAssetBuildFinished, result); });
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

    ProductStatus AssetBuilderManager::PrepareRequest(const Uuid &uuid, const std::string &target, AssetBuildRequest &out, AssetDataBase *database)
    {
        AssetBuildRequest request;
        if (const auto status = (database ? database : AssetDataBase::Get())->PrepareBuildInput(uuid, request); !status)
            return status;
        struct ConfigGuard final : AssetPublicationGuard {
            std::shared_ptr<const AssetPublicationGuard> source;
            std::shared_ptr<std::atomic<uint64_t>>       revision;
            uint64_t                                     expected;
            std::shared_ptr<std::mutex>                  mutex;
            std::vector<NativeFileSystemPtr>             roots;
            std::vector<FilePath>                        inputs;
            std::string                                  fingerprint;
            ProductStatus                                CheckConfiguration() const
            {
                if (revision->load(std::memory_order_acquire) != expected)
                    return {ProductError::Stale, "Cook configuration changed"};
                std::string current;
                if (const auto status = CaptureCookConfiguration(roots, inputs, current); !status)
                    return status;
                return current == fingerprint ? ProductStatus{}
                                              : ProductStatus{ProductError::Stale, "Cook configuration files changed; reload presets"};
            }
            ProductStatus CheckSource(const Uuid &id, const FileSystemPtr &fs, const FilePath &path) const override
            {
                if (const auto status = CheckConfiguration(); !status)
                    return status;
                return source->CheckSource(id, fs, path);
            }
            ProductStatus Acquire(std::unique_ptr<AssetPublicationScope> &scope) const override
            {
                struct Scope final : AssetPublicationScope {
                    std::unique_lock<std::mutex>           lock;
                    std::unique_ptr<AssetPublicationScope> source;
                    explicit Scope(std::mutex &mutex) : lock(mutex)
                    {
                    }
                };
                auto held = std::make_unique<Scope>(*mutex);
                if (const auto status = CheckConfiguration(); !status)
                    return status;
                auto result = source->Acquire(held->source);
                if (result && revision->load(std::memory_order_acquire) != expected) {
                    scope.reset();
                    return {ProductError::Stale, "Cook configuration changed"};
                }
                if (result)
                    scope = std::move(held);
                return result;
            }
        };
        const auto guard = std::make_shared<ConfigGuard>();
        guard->source    = request.publication;
        guard->revision  = configRevision;
        guard->mutex     = configMutex;
        std::vector<std::pair<std::string, std::string>> settings;
        {
            std::lock_guard lock(*configMutex);
            if (!loadedConfigFilesValid)
                return {ProductError::Stale, "Cook presets changed while loading; reload presets"};
            guard->expected    = configRevision->load(std::memory_order_acquire);
            guard->roots       = {engineFs, workSpaceFs};
            guard->inputs      = configInputs;
            guard->fingerprint = loadedConfigFingerprint;
            request.target     = target;
            request.bundle     = config.ResolveBundleForTarget(target);
            request.settings   = config.GetTargetSettings(request.cookJson, target);
            settings = GetBuilderSettings(request.assetInfo->ext, request.bundle, request.settings);
        }
        if (const auto status = guard->CheckConfiguration(); !status)
            return status;
        AppendBuildKey(request.inputFingerprint, guard->fingerprint);
        AppendBuildKey(request.inputFingerprint, request.target);
        AppendBuildKey(request.inputFingerprint, request.bundle);
        std::sort(settings.begin(), settings.end());
        for (const auto &[key, value] : settings) {
            AppendBuildKey(request.inputFingerprint, key);
            AppendBuildKey(request.inputFingerprint, value);
        }
        for (const auto &[key, value] : request.settings) {
            AppendBuildKey(request.inputFingerprint, key);
            AppendBuildKey(request.inputFingerprint, value);
        }
        request.publication   = guard;
        request.localRevision = std::to_string(guard->expected) + ":" + std::to_string(request.sourceRevision);
        out                   = std::move(request);
        return {};
    }

    void AssetBuilderManager::BuildRequest(const Uuid &uuid, const std::string &target, BuildCompletion onFinished)
    {
        Uuid owner;
        if (!AssetDataBase::Get()->GetOwner(uuid, owner)) {
            EmitBuildResult(MakeBuildFailure(uuid, target, "source or generated child is unavailable"), onFinished);
            return;
        }
        if (owner != uuid) {
            BuildRequest(owner, target, [this, uuid, target, onFinished = std::move(onFinished)](const AssetBuildResult &ownerResult) {
                auto result = ownerResult;
                result.uuid = uuid;
                if (result.retCode == AssetBuildRetCode::SUCCESS &&
                    (!AssetDataBase::Get()->Exists(uuid) || !AssetManager::Get()->HasProduct(uuid, GetCookConfig().ResolveBundleForTarget(target)))) {
                    result = MakeBuildFailure(uuid, target, "owner cook did not produce the requested child");
                }
                EmitBuildResult(result, onFinished);
            });
            return;
        }
        AssetBuildRequest request;
        if (const auto status = PrepareRequest(uuid, target, request); !status) {
            EmitBuildResult(MakeBuildFailure(uuid, target, status.Describe()), onFinished);
            return;
        }
        BuildRequest(request, std::move(onFinished));
    }

    void AssetBuilderManager::BuildRequest(const AssetBuildRequest &request, BuildCompletion onFinished)
    {
        if (!request.assetInfo) {
            EmitBuildResult(MakeBuildFailure({}, request.target, "cook request has no source"), onFinished);
            return;
        }
        if (outOfProcessActive) {
            EmitBuildResult(MakeBuildFailure(request.assetInfo->uuid, request.target, "in-process cook disabled while worker is active"), onFinished);
            return;
        }

        const auto key = request.assetInfo->uuid.ToString() + "#" + request.target + "#" + request.inputFingerprint + "#" + request.localRevision;
        {
            std::lock_guard lock(pendingMutex);
            const auto [entry, inserted] = pendingBuilds.try_emplace(key);
            if (onFinished) {
                entry->second.emplace_back(std::move(onFinished));
            }
            if (!inserted) {
                return;
            }
        }
        const auto executionKey = key + "#" + Uuid::Create().ToString();
        auto       build        = request;
        build.assetInfo         = MakeBuildSource(request.assetInfo);
        AssetExecutor::Get()->PushCookTask(executionKey, [this, request = std::move(build), key]() {
            auto *builder = QueryBuilder(request.assetInfo->ext);

            AssetBuildResult result = {};
            result.uuid             = request.assetInfo->uuid;
            result.target           = request.target;
            if (builder == nullptr) {
                result = MakeBuildFailure(request.assetInfo->uuid, request.target, "no builder for extension '" + request.assetInfo->ext + "'");
            } else {
                std::unique_ptr<AssetPublicationScope> scope;
                const auto                             status = request.publication ? request.publication->Acquire(scope) : ProductStatus{};
                scope.reset();
                if (!status)
                    result = MakeBuildFailure(result.uuid, result.target, status.Describe());
                else
                    builder->Request(request, result);
            }
            if (result.retCode == AssetBuildRetCode::SUCCESS && request.publication) {
                std::unique_ptr<AssetPublicationScope> scope;
                if (const auto status = request.publication->Acquire(scope); !status)
                    result = MakeBuildFailure(result.uuid, result.target, status.Describe());
            }
            if (result.retCode == AssetBuildRetCode::SUCCESS) {
                if (const bool published = AssetDataBase::Get()->SetBuildDependencies(request); request.publication && !published)
                    result = MakeBuildFailure(result.uuid, result.target, "Cook source changed before dependency publication");
            }
            std::vector<BuildCompletion> subscribers;
            {
                std::lock_guard lock(pendingMutex);
                const auto      found = pendingBuilds.find(key);
                if (found != pendingBuilds.end()) {
                    subscribers = std::move(found->second);
                    pendingBuilds.erase(found);
                }
            }
            EmitBuildResult(result, [&subscribers](const AssetBuildResult &completed) {
                for (const auto &subscriber : subscribers) {
                    subscriber(completed);
                }
            });
        });
    }

    void AssetBuilderManager::BuildRequestSync(const Uuid &uuid, const std::string &target)
    {
        AssetBuildRequest request;
        if (const auto status = PrepareRequest(uuid, target, request); !status) {
            EmitBuildResult(MakeBuildFailure(uuid, target, status.Describe()), {});
            return;
        }
        auto *builder = QueryBuilder(request.assetInfo->ext);
        if (!builder) {
            EmitBuildResult(MakeBuildFailure(uuid, target, "No builder for source"), {});
            return;
        }
        AssetBuildResult result = {};
        result.uuid             = uuid;
        result.target           = target;
        builder->Request(request, result);
        if (result.retCode == AssetBuildRetCode::SUCCESS && request.publication) {
            std::unique_ptr<AssetPublicationScope> scope;
            if (const auto status = request.publication->Acquire(scope); !status)
                result = MakeBuildFailure(uuid, target, status.Describe());
        }
        if (result.retCode == AssetBuildRetCode::SUCCESS) {
            if (const bool published = AssetDataBase::Get()->SetBuildDependencies(request); request.publication && !published)
                result = MakeBuildFailure(uuid, target, "Cook source changed before dependency publication");
        }

        // Raise the same completion event as the async path so observers (editor cook state) update,
        // even when the sync path is driven by the inline on-demand cook.
        EmitBuildResult(result, {});
    }

    void AssetBuilderManager::RequestCook(const Uuid &uuid, const std::string &target, BuildCompletion onFinished)
    {
        if (outOfProcessActive && cookRunner != nullptr) {
            Uuid owner;
            if (!AssetDataBase::Get()->GetOwner(uuid, owner)) {
                EmitBuildResult(MakeBuildFailure(uuid, target, "source or generated child is unavailable"), onFinished);
                return;
            }
            AssetBuildRequest request;
            if (const auto status = PrepareRequest(owner, target, request); !status) {
                EmitBuildResult(MakeBuildFailure(uuid, target, status.Describe()), onFinished);
                return;
            }
            auto finished = std::make_shared<std::atomic<bool>>(false);
            auto complete = [uuid, owner, target, bundle = request.bundle, publication = request.publication, onFinished = std::move(onFinished),
                             finished](const AssetBuildResult &value) {
                if (finished->exchange(true))
                    return;
                auto result = value;
                result.uuid = uuid;
                if (result.retCode == AssetBuildRetCode::SUCCESS && publication) {
                    std::unique_ptr<AssetPublicationScope> scope;
                    if (const auto status = publication->Acquire(scope); !status)
                        result = MakeBuildFailure(uuid, target, status.Describe());
                }
                if (result.retCode == AssetBuildRetCode::SUCCESS && owner != uuid &&
                    (!AssetDataBase::Get()->RefreshOwner(owner) || !AssetDataBase::Get()->Exists(uuid) ||
                     !AssetManager::Get()->HasProduct(uuid, bundle)))
                    result = MakeBuildFailure(uuid, target, "Owner cook did not publish the requested child");
                if (onFinished)
                    onFinished(result);
            };
            CookJob job{owner, target, request.assetInfo->path.GetStr(), Uuid::Create(), request.inputFingerprint, complete};
            if (!cookRunner->Request(job))
                complete(MakeBuildFailure(uuid, target, "cook worker rejected request"));
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
