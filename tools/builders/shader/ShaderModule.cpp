//
// Created by Zach Lee on 2022/3/12.
//

#include <framework/interface/IModule.h>
#include <framework/interface/Interface.h>
#include <framework/interface/ISystem.h>
#include <framework/application/SettingRegistry.h>
#include <shader/ShaderBuilder.h>
#include <memory>

namespace sky {

    class ShaderBuilderModule : public IModule {
    public:
        ShaderBuilderModule() = default;

        ~ShaderBuilderModule() = default;

        void Init() override
        {
            builder = std::make_unique<ShaderBuilder>();
            Interface<IBuilderRegistry>::Get()->GetApi()->RegisterBuilder(builder.get());

            auto& settings = Interface<ISystemNotify>::Get()->GetApi()->GetSettings();
        }

        void Start() override
        {
        }

        void Stop() override
        {
            Interface<IBuilderRegistry>::Get()->GetApi()->UnRegisterBuilder(builder.get());
        }

        void Tick(float delta) override {}

    private:
        std::unique_ptr<ShaderBuilder> builder;
    };
}

REGISTER_MODULE(sky::ShaderBuilderModule)