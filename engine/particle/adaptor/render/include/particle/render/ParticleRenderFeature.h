#pragma once

#include <aurora/adaptor/renderer/ISceneRenderExtensionSource.h>
#include <aurora/adaptor/renderer/SceneExtensionRetirementQueue.h>
#include <aurora/renderer/IRenderFeature.h>
#include <core/file/FileSystem.h>
#include <particle/ParticleRenderTypes.h>
#include <particle/ParticleView.h>

#include <array>
#include <unordered_map>

namespace sky::particle {
    class ParticleRenderFeature final : public aurora::ISceneRenderExtension {
    public:
        void SetShaderBundle(FileSystemPtr value)
        {
            bundle = std::move(value);
        }
        Name GetName() const override
        {
            return Name("particle.sprite");
        }
        uint32_t GetOrder() const override
        {
            return 100;
        }
        bool RequiresDepth() const override
        {
            return true;
        }
        std::optional<aurora::RendererError> Validate(const aurora::FrameExtensionPayload &) const override;
        aurora::FrameOperationResult         Prepare(aurora::FrameFeaturePrepareContext &) override;
        aurora::FrameOperationResult
             Build(aurora::FrameGraphBuildContext &, const aurora::SceneRenderStageContext &, const aurora::FrameExtensionPayload &) override;
        void OnRetired() override;

    private:
        FileSystemPtr                                bundle;
        aurora::ShaderPtr                            spriteShader;
        aurora::SamplerPtr                           spriteSampler;
        aurora::API                                  shaderBackend = aurora::API::DEFAULT;
        uint64_t                                     shaderEpoch   = 0;
        std::array<aurora::RasterVertexAttribute, 4> attributes;
    };

    // World input and resource publication belong to the host's main-thread packet producer.
    class ParticleRenderSource final : public aurora::ISceneRenderExtensionSource, public IParticleRenderSink, public IParticleViewProvider {
    public:
        explicit ParticleRenderSource(std::shared_ptr<ParticleRenderFeature> feature, aurora::RenderFeatureId id)
            : feature(std::move(feature)), id(id)
        {
        }
        void                                 Submit(std::shared_ptr<const ParticleRenderFrame>) override;
        void                                 RequestRetire();
        void                                 RemoveWorld(const Uuid &) override;
        bool                                 GetView(const Uuid &, ParticleViewInfo &) const override;
        std::optional<aurora::RendererError> Append(aurora::FramePacket &, const aurora::SceneExtensionSourceContext &) override;

    private:
        std::optional<aurora::RendererError>
        BuildAdditions(const aurora::SceneExtensionSourceContext &, std::span<const aurora::RenderResourceKey>, aurora::FramePacket &,
                       std::unordered_map<Uuid, ParticleViewInfo> &, std::vector<aurora::RenderResourceKey> &);
        void MergeAdditions(aurora::FramePacket &, aurora::FramePacket &&);

        std::shared_ptr<ParticleRenderFeature>                               feature;
        aurora::RenderFeatureId                                              id;
        aurora::RenderFeatureId                                              registeredScene;
        uint64_t                                                             registeredEpoch       = 0;
        uint64_t                                                             registeredDeviceEpoch = 0;
        std::unordered_map<Uuid, std::shared_ptr<const ParticleRenderFrame>> frames;
        std::unordered_map<Uuid, ParticleViewInfo>                           views;
        std::vector<aurora::RenderResourceKey>                               published;
        Uuid                                                                 whiteId = Uuid::Create();
        std::shared_ptr<const aurora::RenderResourcePayload>                 whitePayload;
        uint64_t                                                             whiteDeviceEpoch = 0;
    };
} // namespace sky::particle
