#include <aurora/adaptor/renderer/SceneRenderExtensionSourceRegistry.h>
#include <framework/interface/IModule.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <particle/ParticleView.h>
#include <particle/render/ParticleRenderFeature.h>

namespace sky::particle {
    namespace {
        // Engine-reserved scene extension feature id; keep unique against other extensions.
        constexpr uint32_t PARTICLE_EXTENSION_FEATURE_INDEX = 9;
    } // namespace

    class ParticleRenderModule final : public IModule {
    public:
        bool Init(const StartArguments &) override
        {
            if (feature || Interface<IParticleRenderSink>::Get()->GetApi() || Interface<IParticleViewProvider>::Get()->GetApi())
                return false;
            feature = std::make_shared<ParticleRenderFeature>();
            if (Platform::Get())
                feature->SetShaderBundle(Platform::Get()->GetBundleFileSystem());
            source = std::make_unique<ParticleRenderSource>(feature, aurora::RenderFeatureId{PARTICLE_EXTENSION_FEATURE_INDEX, ++extensionGeneration});
            if (!aurora::SceneRenderExtensionSourceRegistry::Get()->Register(*source))
                return false;
            Interface<IParticleRenderSink>::Get()->Register(*source);
            Interface<IParticleViewProvider>::Get()->Register(*source);
            return true;
        }
        void Shutdown() override
        {
            if (!feature)
                return;
            source->RequestRetire();
            aurora::SceneRenderExtensionSourceRegistry::Get()->UnRegister(*source);
            if (Interface<IParticleViewProvider>::Get()->GetApi() == source.get())
                Interface<IParticleViewProvider>::Get()->UnRegister();
            if (Interface<IParticleRenderSink>::Get()->GetApi() == source.get())
                Interface<IParticleRenderSink>::Get()->UnRegister();
            source.reset();
            feature.reset();
        }

    private:
        std::shared_ptr<ParticleRenderFeature> feature;
        std::unique_ptr<ParticleRenderSource>  source;
        uint32_t                               extensionGeneration = 0;
    };
} // namespace sky::particle
REGISTER_MODULE_TAGGED(sky::particle::ParticleRenderModule)
