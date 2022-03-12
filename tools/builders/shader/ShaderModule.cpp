//
// Created by Zach Lee on 2022/3/12.
//

#include <framework/interface/AppInterface.h>

namespace sky {

    class ShaderBuilderModule : public IModule {
    public:
        ShaderBuilderModule() = default;
        ~ShaderBuilderModule() = default;

        void Start() override
        {

        }

        void Stop() override
        {

        }

        void Tick(float delta) override {}
    };

}

REGISTER_MODULE(sky::ShaderBuilderModule)