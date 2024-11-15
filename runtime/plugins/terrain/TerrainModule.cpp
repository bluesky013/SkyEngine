//
// Created by blues on 2024/11/7.
//

#include <framework/interface/IModule.h>

namespace sky {

    class TerrainModule : public IModule {
    public:
        TerrainModule() = default;
        ~TerrainModule() override = default;

        bool Init(const StartArguments &args) override
        {
            return true;
        }

        void Start() override
        {
        }

        void Shutdown() override
        {
        }
    };

} // namespace sky

REGISTER_MODULE(sky::TerrainModule)