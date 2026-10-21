#pragma once

#include "HostSceneFixture.h"
#include <array>
#include <aurora/renderer/IRenderFeature.h>

namespace sky::aurora {
    class DepthSamplingFeature final : public IFrameGraphFeature {
    public:
        explicit DepthSamplingFeature(RendererInfo info)
        {
            codes[0] = CompileHostRaster(info, R"(
                float4 VSMain(uint id : SV_VertexID) : SV_Position {
                    float2 p = float2((id << 1) & 2, id & 2);
                    return float4(p * 2 - 1, 0.25, 1);
                }
                void FSMain() {}
            )");
            codes[1] = CompileHostRaster(info, R"(
                Texture2DArray<float> depthImage : register(t0, space1);
                SamplerState depthSampler : register(s1, space1);
                float4 VSMain(uint id : SV_VertexID) : SV_Position {
                    float2 p = float2((id << 1) & 2, id & 2);
                    return float4(p * 2 - 1, 0, 1);
                }
                float4 FSMain(float4 p : SV_Position) : SV_Target {
                    float d = depthImage.SampleLevel(depthSampler, float3(p.xy / 8, 0), 0);
                    return float4(d, d, d, 1);
                }
            )");
        }
        Name GetName() const override
        {
            return Name("depth.sample");
        }
        FrameOperationResult BuildFrame(FrameGraphBuildContext &context, const FrameExtensionPayload &payload) override
        {
            if (payload.outputs.size() != 1)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            const auto output = context.GetOutput(payload.outputs.front());
            if (!output)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            for (uint32_t i = 0; i < 2; ++i) {
                if (shaders[i])
                    continue;
                auto made = context.CreateShader(codes[i]);
                if (const auto *error = std::get_if<FrameError>(&made))
                    return *error;
                shaders[i] = std::get<ShaderPtr>(made);
            }
            if (!sampler) {
                Sampler::Descriptor desc;
                desc.minFilter = desc.magFilter = Filter::NEAREST;
                desc.mipmapMode                 = MipFilter::NEAREST;
                auto made                       = context.CreateSampler(desc);
                if (const auto *error = std::get_if<FrameError>(&made))
                    return *error;
                sampler = std::get<SamplerPtr>(made);
            }
            Image::Descriptor desc;
            desc.extent    = {16, 16, 1};
            desc.mipLevels = desc.arrayLayers = 2;
            desc.format                       = PixelFormat::D32;
            desc.usage                        = ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::SAMPLED;
            auto made                         = context.CreateImage(desc);
            if (const auto *error = std::get_if<FrameError>(&made))
                return *error;
            const auto      depth = std::get<GraphImageHandle>(made);
            FrameRasterPass write;
            write.depth = RasterDepthAttachment{depth, LoadOp::CLEAR, StoreOp::STORE, 1, {1, 1}};
            RasterDraw draw;
            draw.shader              = shaders[0];
            draw.command.vertexCount = 3;
            draw.depth               = {true, true, CompareOp::LESS_OR_EQUAL};
            draw.scissor             = {{0, 0}, {4, 8}};
            write.draws.assign(70, draw);
            auto result = context.Raster(write);
            if (std::holds_alternative<FrameError>(result))
                return result;
            write.depth->load = LoadOp::LOAD;
            write.draws.clear();
            result = context.Raster(std::move(write));
            if (std::holds_alternative<FrameError>(result))
                return result;
            FrameRasterPass sample;
            sample.color             = output->color;
            draw                     = {};
            draw.shader              = shaders[1];
            draw.command.vertexCount = 3;
            draw.images              = {{1, 0, depth, {ImageViewType::VIEW_2D_ARRAY, 1, 1, 1, 1}}};
            draw.samplers            = {{1, 1, sampler}};
            sample.draws.push_back(std::move(draw));
            return context.Raster(std::move(sample));
        }
        void OnRetired() override
        {
            shaders = {};
            sampler.Reset(nullptr);
        }

    private:
        std::array<RenderMaterialData, 2> codes;
        std::array<ShaderPtr, 2>          shaders;
        SamplerPtr                        sampler;
    };
} // namespace sky::aurora
