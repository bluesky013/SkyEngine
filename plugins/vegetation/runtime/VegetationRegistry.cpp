//
// Vegetation feature plugin module: registers the vegetation component and asset handling.
//

#include <vegetation/VegetationAsset.h>
#include <vegetation/components/VegetationComponent.h>

#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::vegetation {

    class VegetationModule : public IModule {
    public:
        VegetationModule()           = default;
        ~VegetationModule() override = default;

        bool Init(const StartArguments &args) override { return true; }

        void Start() override
        {
            auto *context = SerializationContext::Get();
            VegetationComponent::Reflect(context);
            RegisterVegetationAssetType();
        }

        void Shutdown() override {}
    };

} // namespace sky::vegetation
REGISTER_MODULE(sky::vegetation::VegetationModule)
