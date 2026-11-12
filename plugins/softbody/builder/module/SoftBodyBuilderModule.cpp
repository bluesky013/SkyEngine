#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <physics/softbody/IMeshAuthoringCompiler.h>
#include <softbody/builder/SoftBodyBuilder.h>

namespace sky::softbody {
    class SoftBodyBuilderModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            AssetBuilderManager::Get()->RegisterBuilder(new SoftBodyBuilder());
        }
        bool Init(const StartArguments &) override
        {
            return static_cast<bool>(phy::MeshAuthoringCompilerRegistry::Get()->Acquire());
        }
    };
} // namespace sky::softbody
REGISTER_MODULE_TAGGED(sky::softbody::SoftBodyBuilderModule)
