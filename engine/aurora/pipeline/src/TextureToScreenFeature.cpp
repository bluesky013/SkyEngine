#include <aurora/pipeline/TextureToScreenFeature.h>

#include "TextureToScreenPass.h"
#include <aurora/renderer/PassResourceBindings.h>
#include <cstring>

namespace sky::aurora {
    namespace {
        struct Input {
            shared::TextureToScreenParams params;
            uint64_t                      revision = 0;
        };
        class TextureToScreenFeature final : public IFrameGraphFeature {
        public:
            TextureToScreenFeature(Name name, RenderTechniqueData code) : name(name), code(std::move(code))
            {
            }
            Name GetName() const override
            {
                return name;
            }
            std::optional<RendererError> Validate(const FrameExtensionPayload &payload) const override
            {
                if (payload.bytes.size() != sizeof(Input) ||
                    !((payload.outputs.size() == 2 && payload.resources.empty()) || (payload.outputs.size() == 1 && payload.resources.size() == 1)))
                    return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Texture pass requires one output and one image source"};
                return {};
            }
            FrameOperationResult BuildFrame(FrameGraphBuildContext &context, const FrameExtensionPayload &payload) override
            {
                if (const auto error = Validate(payload))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, error->message};
                Input input{};
                std::memcpy(&input, payload.bytes.data(), sizeof(input));
                const auto output = context.GetOutput(payload.outputs.front());
                const auto image  = payload.resources.empty() ? context.ResolveViewOutput({payload.outputs.back(), input.revision})
                                                              : context.ResolveImage(payload.resources.front());
                if (!output || !image)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Texture pass input or output is unavailable"};
                if (!shader) {
                    const auto created = context.CreateShader(code);
                    if (const auto *error = std::get_if<FrameError>(&created))
                        return *error;
                    shader = std::get<ShaderPtr>(created);
                }
                if (!sampler) {
                    Sampler::Descriptor descriptor;
                    descriptor.addressModeU = descriptor.addressModeV = descriptor.addressModeW = WrapMode::CLAMP_TO_EDGE;
                    const auto created                                                          = context.CreateSampler(descriptor);
                    if (const auto *error = std::get_if<FrameError>(&created))
                        return *error;
                    sampler = std::get<SamplerPtr>(created);
                }
                const auto slot    = shared::TextureToScreenPassBindings::gTextureToScreen;
                const auto written = context.WriteUniform(slot.set, slot.binding, std::as_bytes(std::span{&input.params, 1}));
                if (const auto *error = std::get_if<FrameError>(&written))
                    return *error;
                PassResourceBindings bindings(shared::GetTextureToScreenPassReflection());
                const auto           uniform = bindings.Uniform(slot, std::get<RasterUniformBinding>(written).range);
                if (std::holds_alternative<FrameError>(uniform))
                    return uniform;
                const auto sampled = bindings.Image(shared::TextureToScreenPassBindings::gSourceTexture, *image);
                if (std::holds_alternative<FrameError>(sampled))
                    return sampled;
                const auto sampling = bindings.Sampler(shared::TextureToScreenPassBindings::gSourceSampler, sampler);
                if (std::holds_alternative<FrameError>(sampling))
                    return sampling;
                RasterDraw draw;
                draw.shader              = shader;
                draw.command.vertexCount = 3;
                draw.scissor.extent      = output->output.extent;
                const PassResourceBindings *groups[]{&bindings};
                if (const auto result = PassResourceBindings::Apply(draw, groups); std::holds_alternative<FrameError>(result))
                    return result;
                FrameRasterPass pass;
                pass.color = output->color;
                pass.draws.push_back(std::move(draw));
                return context.Fullscreen(std::move(pass));
            }
            void OnRetired() override
            {
                shader  = {};
                sampler = {};
            }

        private:
            Name                name;
            RenderTechniqueData code;
            ShaderPtr           shader;
            SamplerPtr          sampler;
        };

        FrameExtensionPayload Payload(Name feature, ViewportId output, Vector4 tint, uint64_t revision)
        {
            FrameExtensionPayload payload;
            payload.feature = feature;
            payload.outputs = {output};
            const Input input{{tint}, revision};
            payload.bytes.resize(sizeof(input));
            std::memcpy(payload.bytes.data(), &input, sizeof(input));
            return payload;
        }
    } // namespace

    std::shared_ptr<IFrameGraphFeature> CreateTextureToScreenFeature(Name name, RenderTechniqueData shader)
    {
        return std::make_shared<TextureToScreenFeature>(name, std::move(shader));
    }
    FrameExtensionPayload TextureToScreenInput(Name feature, ViewportId output, ViewOutputRef source, Vector4 tint)
    {
        auto payload = Payload(feature, output, tint, source.revision);
        payload.outputs.push_back(source.viewport);
        return payload;
    }
    FrameExtensionPayload TextureToScreenInput(Name feature, ViewportId output, RenderResourceKey source, Vector4 tint)
    {
        auto payload = Payload(feature, output, tint, 0);
        payload.resources.push_back(source);
        return payload;
    }
} // namespace sky::aurora
