#include <particle/render/ParticleRenderFeature.h>

#include "ParticleSpritePass.h"
#include <particle/render/ParticleRenderPayload.h>
#include <particle/render/ParticleSpritePacking.h>

#include <algorithm>
#include <span>
#include <aurora/renderer/PassResourceBindings.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderTechnique.h>
#include <cmath>

namespace sky::particle {
    namespace {
        aurora::BlendState BlendFor(ParticleBlendMode mode)
        {
            aurora::BlendState blend;
            blend.blendEn      = true;
            blend.writeMask    = 0xF;
            blend.colorBlendOp = aurora::BlendOp::ADD;
            blend.alphaBlendOp = aurora::BlendOp::ADD;
            switch (mode) {
            case ParticleBlendMode::ADDITIVE:
                blend.srcColor = aurora::BlendFactor::SRC_ALPHA;
                blend.dstColor = aurora::BlendFactor::ONE;
                blend.srcAlpha = aurora::BlendFactor::ONE;
                blend.dstAlpha = aurora::BlendFactor::ONE;
                break;
            case ParticleBlendMode::PREMULTIPLIED:
                blend.srcColor = aurora::BlendFactor::ONE;
                blend.dstColor = aurora::BlendFactor::ONE_MINUS_SRC_ALPHA;
                blend.srcAlpha = aurora::BlendFactor::ONE;
                blend.dstAlpha = aurora::BlendFactor::ONE_MINUS_SRC_ALPHA;
                break;
            case ParticleBlendMode::ALPHA:
            default:
                blend.srcColor = aurora::BlendFactor::SRC_ALPHA;
                blend.dstColor = aurora::BlendFactor::ONE_MINUS_SRC_ALPHA;
                blend.srcAlpha = aurora::BlendFactor::ONE;
                blend.dstAlpha = aurora::BlendFactor::ONE_MINUS_SRC_ALPHA;
                break;
            }
            return blend;
        }
    } // namespace

    std::optional<aurora::RendererError> ParticleRenderFeature::Validate(const aurora::FrameExtensionPayload &payload) const
    {
        if (payload.feature != GetName())
            return aurora::RendererError{aurora::RendererErrorCode::INVALID_PACKET, "Invalid particle render payload"};
        return ValidateParticleRenderPayload(payload);
    }

    aurora::FrameOperationResult ParticleRenderFeature::Prepare(aurora::FrameFeaturePrepareContext &context)
    {
        const auto info = context.GetRendererInfo();
        if (spriteShader && shaderBackend == info.backend && shaderEpoch == info.deviceEpoch)
            return aurora::FrameOperationComplete{};
        if (!bundle)
            return aurora::FrameError{aurora::FrameErrorCode::INVALID_STATE, "Particle shader bundle is unavailable"};
#if SKY_DEVELOP
        constexpr auto mode = aurora::ShaderResourceMode::DEVELOPMENT;
#else
        constexpr auto mode = aurora::ShaderResourceMode::PRODUCTS;
#endif
        auto resources = aurora::LoadShaderResources(*bundle, info.backend, mode);
        if (const auto *error = std::get_if<std::string>(&resources))
            return aurora::FrameError{aurora::FrameErrorCode::INVALID_ARGUMENT, *error};
        auto &files = std::get<aurora::ShaderResources>(resources);
        if (!files.sources)
            return aurora::FrameError{aurora::FrameErrorCode::INVALID_STATE, "Particle shader sources are unavailable"};
        aurora::ShaderTechnique technique;
        technique.vertex   = {.relativePath = "particle/ParticleSprite.slang", .entry = "VSMain"};
        technique.fragment = {.relativePath = "particle/ParticleSprite.slang", .entry = "FSMain"};
        auto compiled      = aurora::CompileShaderTechnique(*files.sources, info.backend, technique, files.products.Get());
        if (const auto *error = std::get_if<aurora::RendererError>(&compiled))
            return aurora::FrameError{aurora::FrameErrorCode::INVALID_ARGUMENT, error->message};
        auto shader = context.CreateShader(std::get<aurora::RenderTechniqueData>(compiled));
        if (const auto *error = std::get_if<aurora::FrameError>(&shader))
            return *error;
        const auto                                  &preparedShader = std::get<aurora::ShaderPtr>(shader);
        std::array<aurora::RasterVertexAttribute, 4> preparedAttributes;
        for (uint32_t index = 0; index < preparedAttributes.size(); ++index) {
            const auto &inputs = preparedShader->GetReflection().vertexInputs;
            const auto  found  = std::find_if(inputs.begin(), inputs.end(), [&](const auto &input) {
                return (input.semantic == "INSTANCE" && input.semanticIndex == index) || input.semantic == "INSTANCE" + std::to_string(index);
            });
            if (found == inputs.end())
                return aurora::FrameError{aurora::FrameErrorCode::INVALID_ARGUMENT, "Particle shader instance input is missing"};
            preparedAttributes[index] = {found->location, 0, index * 16u, aurora::Format::F_RGBA32, "INSTANCE", index};
        }
        attributes    = std::move(preparedAttributes);
        spriteSampler = nullptr;
        spriteShader  = preparedShader;
        shaderBackend = info.backend;
        shaderEpoch   = info.deviceEpoch;
        return aurora::FrameOperationComplete{};
    }

    aurora::FrameOperationResult ParticleRenderFeature::Build(aurora::FrameGraphBuildContext        &context,
                                                              const aurora::SceneRenderStageContext &stage,
                                                              const aurora::FrameExtensionPayload   &payload)
    {
        if (!spriteShader)
            return aurora::FrameError{aurora::FrameErrorCode::INVALID_STATE, "Particle sprite shader is not prepared"};
        const auto target = context.GetOutput(stage.view.view.output);
        if (!target || !stage.depth.graph)
            return aurora::FrameError{aurora::FrameErrorCode::INVALID_ARGUMENT, "Particle scene attachments are unavailable"};
        auto sceneToLogical = aurora::RenderIdentityMatrix();
        sceneToLogical.v[5] = -1.f;
        shared::ParticleSpriteParams params;
        params.viewProjection  = target->logicalToClip * sceneToLogical * stage.view.view.projection * stage.view.view.view;
        const auto viewInverse = stage.view.view.view.Inverse();
        params.cameraRight     = {viewInverse.v[0], viewInverse.v[1], viewInverse.v[2], 0.f};
        params.cameraUp        = {viewInverse.v[4], viewInverse.v[5], viewInverse.v[6], 0.f};
        aurora::FrameRasterPass pass;
        pass.color = stage.color;
        pass.load  = aurora::LoadOp::LOAD;
        pass.store = aurora::StoreOp::STORE;
        pass.depth = aurora::RasterDepthAttachment{stage.depth, aurora::LoadOp::LOAD, aurora::StoreOp::STORE, 1.f};

        ParticleRenderPayloadReader reader(std::as_bytes(std::span{payload.bytes}));
        ParticleRenderPayloadEmitter emitter;
        while (reader.Next(emitter)) {
            if (!emitter.count || emitter.texture >= payload.resources.size())
                continue;
            const auto image = context.ResolveImage(payload.resources[emitter.texture]);
            if (!image)
                continue;
            auto uploaded = context.Upload(emitter.instances, aurora::BufferUsageFlagBit::VERTEX);
            if (const auto *error = std::get_if<aurora::FrameError>(&uploaded))
                return *error;
            if (!spriteSampler) {
                aurora::Sampler::Descriptor sampler;
                sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = aurora::WrapMode::CLAMP_TO_EDGE;
                auto created = context.CreateSampler(sampler);
                if (const auto *error = std::get_if<aurora::FrameError>(&created))
                    return *error;
                spriteSampler = std::get<aurora::SamplerPtr>(created);
            }
            aurora::RasterDraw draw;
            draw.shader  = spriteShader;
            draw.blend   = BlendFor(emitter.blend);
            draw.command = {6, emitter.count, 0, 0};
            draw.scissor = {{0, 0}, stage.extent};
            draw.vertices.push_back({std::get<aurora::RasterBufferRange>(uploaded), sizeof(ParticleSpriteInstance), aurora::VertexInputRate::PER_INSTANCE});
            draw.attributes.assign(attributes.begin(), attributes.end());
            draw.depth = {true, false, aurora::CompareOp::LESS_OR_EQUAL};
            aurora::PassResourceBindings bindings(shared::GetParticleSpritePassReflection());
            if (const auto bound = bindings.UniformData(shared::ParticleSpritePassBindings::gParticleSprite, params);
                std::holds_alternative<aurora::FrameError>(bound))
                return bound;
            if (const auto bound = bindings.Image(shared::ParticleSpritePassBindings::gSpriteTexture, *image);
                std::holds_alternative<aurora::FrameError>(bound))
                return bound;
            if (const auto bound = bindings.Sampler(shared::ParticleSpritePassBindings::gSpriteSampler, spriteSampler);
                std::holds_alternative<aurora::FrameError>(bound))
                return bound;
            const aurora::PassResourceBindings *groups[] = {&bindings};
            if (const auto applied = aurora::PassResourceBindings::Apply(context, draw, groups); std::holds_alternative<aurora::FrameError>(applied))
                return applied;
            pass.draws.push_back(std::move(draw));
        }
        if (pass.draws.empty())
            return aurora::FrameOperationComplete{};
        return context.Raster(std::move(pass));
    }
    void ParticleRenderFeature::OnRetired()
    {
        spriteShader  = nullptr;
        spriteSampler = nullptr;
        shaderBackend = aurora::API::DEFAULT;
        shaderEpoch   = 0;
        attributes    = {};
    }
} // namespace sky::particle
