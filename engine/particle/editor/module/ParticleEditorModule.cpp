#include <editor/ui/AssetViewProvider.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>
#include <particle/assets/ParticleEffectAsset.h>
#include <particle/editor/ParticleEditor.h>

namespace sky::particle::editor {
    class ParticleEditorModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterParticleAuthoringTypes(SerializationContext::Get());
            RegisterParticleAssetViews(SerializationContext::Get());
        }
        bool Init(const StartArguments &) override
        {
            if (provider || sky::editor::AssetViewProviderRegistry::Get()->Find("ParticleEffect"))
                return false;
            provider = CreateParticleViewProvider();
            if (!provider)
                return false;
            sky::editor::AssetViewProviderRegistry::Get()->Register("ParticleEffect", provider.get());
            RegisterParticleAssetViews();
            return true;
        }
        void Shutdown() override
        {
            if (!provider)
                return;
            UnregisterParticleAssetViews();
            sky::editor::AssetViewProviderRegistry::Get()->Unregister("ParticleEffect", provider.get());
            provider.reset();
        }

    private:
        std::unique_ptr<sky::editor::IAssetViewProvider> provider;
    };
} // namespace sky::particle::editor
REGISTER_MODULE_TAGGED(sky::particle::editor::ParticleEditorModule)
