//
// Created on 2026/10/17.
//

#pragma once

#include "graph/FrameGraphBuilder.h"
#include "graph/PreparedRaster.h"
#include <algorithm>
#include <aurora/renderer/IRenderFeature.h>
#include <tuple>

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
        std::variant<FrameError, FrameBindingGroupUse>
        MaterializeParameters(ShaderPtr, uint32_t, const ShaderParameterTransactionState &, RasterBindingDraft) override;
        // The coordinator retains the catalog in the frozen graph; this view borrow ends before BuildFrame.
        void SetPreparedView(const PreparedViewData *,
                             const FrozenResourceCatalog &,
                             std::span<const PreparedMeshLayout>,
                             std::span<const PreparedMaterialTechniques>,
                             std::span<const PreparedGraphBuffer>);
        std::variant<FrameError, std::vector<ResolvedRasterDraw>>
        ResolveDraws(Name, Rect2D, PreparedDrawList, const ShaderVariant *pipelineValues = nullptr) override;
        std::variant<FrameError, GraphBufferHandle> CreateBuffer(const Buffer::Descriptor &) override;
        std::variant<FrameError, GraphImageHandle>  CreateImage(const Image::Descriptor &) override;
        FrameOperationResult                        Copy(BufferCopyPass) override;
        FrameOperationResult                        Copy(BufferImageCopyPass) override;
        FrameOperationResult                        Raster(FrameRasterPass) override;
        FrameOperationResult                        Fullscreen(FrameRasterPass) override;
        FrameOperationResult                        Compute(FrameComputePass) override;
        FrameOperationResult                        Custom(FrameCustomPass) override;
        FrameOperationResult                        Export(GraphBufferExport) override;
        FrameOperationResult                        Export(GraphImageHandle) override;
        FrameOperationResult                        ExportOutputs();

    private:
        struct ParameterGroupKey {
            ShaderPtr                                                  shader;
            uint32_t                                                   set = 0;
            std::map<uint32_t, std::map<std::string, ShaderDataValue>> fields;
            std::shared_ptr<const FrameBindingGroup>                   resources;
        };
        struct ParameterGroupLess {
            bool operator()(const ParameterGroupKey &, const ParameterGroupKey &) const;
        };
        struct UniformUploadView {
            uint32_t                   set = 0, binding = 0;
            ShaderResourceType         type = ShaderResourceType::UNIFORM_BUFFER;
            std::span<const std::byte> bytes;
            UniformUploadView          View() const
            {
                return *this;
            }
        };
        struct UniformUploadKey {
            uint32_t               set = 0, binding = 0;
            ShaderResourceType     type = ShaderResourceType::UNIFORM_BUFFER;
            std::vector<std::byte> bytes;
            UniformUploadView      View() const
            {
                return {set, binding, type, bytes};
            }
        };
        struct UniformUploadLess {
            using is_transparent = void;
            template <class A, class B>
            bool operator()(const A &left, const B &right) const
            {
                const auto a = left.View(), b = right.View();
                const auto ak = std::tuple{a.set, a.binding, a.type}, bk = std::tuple{b.set, b.binding, b.type};
                return ak != bk ? ak < bk : std::lexicographical_compare(a.bytes.begin(), a.bytes.end(), b.bytes.begin(), b.bytes.end());
            }
        };
        struct Output {
            FrameRenderTarget target;
            bool              sampleable = false;
        };
        FrameOperationResult                                                  AddRaster(FrameRasterPass, bool fullscreen);
        Device                                                               &device;
        DeviceFrameContext                                                   &context;
        FrameGraphBuilder                                                    &builder;
        const FrameBuildInputs                                               &inputs;
        std::vector<Output>                                                   outputs;
        std::vector<std::pair<RenderResourceKey, GraphImageHandle>>           images;
        const PreparedViewData                                               *view      = nullptr;
        const FrozenResourceCatalog                                          *resources = nullptr;
        std::span<const PreparedMeshLayout>                                   meshes;
        std::span<const PreparedMaterialTechniques>                           materials;
        std::span<const PreparedGraphBuffer>                                  localBuffers;
        std::map<ParameterGroupKey, FrameBindingGroupUse, ParameterGroupLess> parameterGroups;
        std::map<UniformUploadKey, RasterUniformBinding, UniformUploadLess>   uniformUploads;
    };
} // namespace sky::aurora
