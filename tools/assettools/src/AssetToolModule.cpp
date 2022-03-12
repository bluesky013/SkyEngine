//
// Created by Zach Lee on 2022/3/12.
//

#include <framework/interface/IModule.h>
#include <framework/interface/IBuilder.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/application/SettingRegistry.h>
#include <core/logger/Logger.h>
#include <set>

static const char* TAG = "AssetTool";

namespace sky {

    class AssetToolModule
        : public IModule
        , public IBuilderRegistry {
    public:
        AssetToolModule() = default;
        ~AssetToolModule() = default;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        void RegisterBuilder(IBuilder* builder) override;
        void UnRegisterBuilder(IBuilder* builder) override;

    private:
        std::set<IBuilder*> builders;
    };

    void AssetToolModule::Start()
    {
        Interface<IBuilderRegistry>::Get()->Register(*this);
    }

    void AssetToolModule::Stop()
    {
        Interface<IBuilderRegistry>::Get()->UnRegister();
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

    void AssetToolModule::Tick(float delta)
    {
        auto& settings = Interface<ISystemNotify>::Get()->GetApi()->GetSettings();
        std::string out;
        settings.Save(out);
    }
}

REGISTER_MODULE(sky::AssetToolModule)
