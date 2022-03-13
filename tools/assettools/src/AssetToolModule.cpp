//
// Created by Zach Lee on 2022/3/12.
//

#include <framework/interface/IModule.h>
#include <framework/interface/IBuilder.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/SettingRegistry.h>
#include <framework/task/TaskManager.h>
#include <core/logger/Logger.h>
#include <set>
#include <filesystem>

static const char* TAG = "AssetTool";

namespace sky {

    class BuildTask {

    };

    class AssetToolModule
        : public IModule
        , public IBuilderRegistry {
    public:
        AssetToolModule();
        ~AssetToolModule();

        void Init() override {}

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        void RegisterBuilder(IBuilder* builder) override;
        void UnRegisterBuilder(IBuilder* builder) override;

    private:
        IBuilder* FindBuilder(const std::string& extension);

        std::set<IBuilder*> builders;
        TaskFlow taskFlow;
    };

    AssetToolModule::AssetToolModule()
    {
        Interface<IBuilderRegistry>::Get()->Register(*this);
    }

    AssetToolModule::~AssetToolModule()
    {
        Interface<IBuilderRegistry>::Get()->UnRegister();
    }

    void AssetToolModule::Start()
    {
        auto sysApi = Interface<ISystemNotify>::Get()->GetApi();
        auto& settings = sysApi->GetSettings();

        std::filesystem::path projectRoot(settings.VisitString("project_root"));

        settings.VisitStringArray("asset_folders", [&projectRoot, this](const char* folder) {
            std::filesystem::path path = projectRoot;
            path.append(folder);
            for (auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                auto file = std::filesystem::absolute(entry.path());
                auto builder = FindBuilder(file.extension().string());
                if (builder != nullptr) {
                    BuildRequest request = {
                        file
                    };
                    builder->Build(request);
                }
            }
        });

        sysApi->SetExit();
    }

    void AssetToolModule::Stop()
    {

    }

    void AssetToolModule::RegisterBuilder(IBuilder* builder)
    {
        if (builder != nullptr) {
            builders.emplace(builder);
        }
    }

    void AssetToolModule::UnRegisterBuilder(IBuilder* builder)
    {
        auto iter = builders.find(builder);
        if (iter != builders.end()) {
            builders.erase(iter);
        }
    }

    IBuilder* AssetToolModule::FindBuilder(const std::string& extension)
    {
        auto iter = std::find_if(builders.begin(), builders.end(), [&extension](const IBuilder* builder) {
            return builder->Support(extension);
        });
        if (iter != builders.end()) {
            return *iter;
        }
        return nullptr;
    }

    void AssetToolModule::Tick(float delta)
    {
    }
}

REGISTER_MODULE(sky::AssetToolModule)
