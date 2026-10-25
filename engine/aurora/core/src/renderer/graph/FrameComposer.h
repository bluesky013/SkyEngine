//
// Created on 2026/10/17.
//

#pragma once

#include "graph/FrameGraphBuilder.h"
#include "graph/PreparedRaster.h"
#include <aurora/renderer/IRenderFeature.h>

namespace sky::aurora {
    class Device;
    class DeviceFrameContext;
    std::optional<FrameError>           ValidateFrameShader(const RenderTechniqueData &);
    std::variant<FrameError, ShaderPtr> CreateFrameShader(Device &, const RenderTechniqueData &);

    struct PreparedMaterialTechniques {
        RenderResourceKey                                    resource;
        std::vector<std::pair<Name, PreparedMaterialLayout>> techniques;
    };

    class FrameComposer final : public FrameGraphBuildContext {
    public:
        FrameComposer(Device &, DeviceFrameContext &, FrameGraphBuilder &, const FrameBuildInputs &);
        void                                        AddImage(RenderResourceKey, GraphImageHandle);
        std::optional<GraphImageHandle>             ResolveImage(const RenderResourceKey &) const override;
        void                                        AddOutput(FrameRenderTarget, bool sampleable);
        const FrameBuildInputs                     &GetInputs() const override;
        std::optional<FrameRenderTarget>            GetOutput(ViewportId) const override;
        std::optional<GraphImageHandle>             ResolveViewOutput(ViewOutputRef) const override;
        std::variant<FrameError, ShaderPtr>         CreateShader(const RenderTechniqueData &) override;
        std::variant<FrameError, SamplerPtr>        CreateSampler(const Sampler::Descriptor &) override;
        std::variant<FrameError, RasterBufferRange> Upload(std::span<const std::byte>, BufferUsageFlags) override;
        std::variant<FrameError, RasterUniformBinding>
        WriteUniform(uint32_t, uint32_t, std::span<const std::byte>, ShaderResourceType = ShaderResourceType::UNIFORM_BUFFER) override;
        // The coordinator retains the catalog in the frozen graph; this view borrow ends before BuildFrame.
        void                                                      SetPreparedView(const PreparedViewData *,
                                                                                  const FrozenResourceCatalog &,
                                                                                  std::span<const PreparedMeshLayout>,
                                                                                  std::span<const PreparedMaterialTechniques>,
                                                                                  std::span<const PreparedGraphBuffer>);
        std::variant<FrameError, std::vector<ResolvedRasterDraw>>
        ResolveDraws(Name, Rect2D, PreparedDrawList, const ShaderVariant *pipelineValues = nullptr) override;
        std::variant<FrameError, GraphImageHandle>                CreateImage(const Image::Descriptor &) override;
        FrameOperationResult                                      Raster(FrameRasterPass) override;
        FrameOperationResult                                      Fullscreen(FrameRasterPass) override;
        FrameOperationResult                                      Compute(FrameComputePass) override;
        FrameOperationResult                                      ExportOutputs();

    private:
        struct Output {
            FrameRenderTarget target;
            bool              sampleable = false;
        };
        FrameOperationResult                                        AddRaster(FrameRasterPass, bool fullscreen);
        Device                                                     &device;
        DeviceFrameContext                                         &context;
        FrameGraphBuilder                                          &builder;
        const FrameBuildInputs                                     &inputs;
        std::vector<Output>                                         outputs;
        std::vector<std::pair<RenderResourceKey, GraphImageHandle>> images;
        const PreparedViewData                                     *view      = nullptr;
        const FrozenResourceCatalog                                *resources = nullptr;
        std::span<const PreparedMeshLayout>                         meshes;
        std::span<const PreparedMaterialTechniques>                 materials;
        std::span<const PreparedGraphBuffer>                        localBuffers;
    };
} // namespace sky::aurora
