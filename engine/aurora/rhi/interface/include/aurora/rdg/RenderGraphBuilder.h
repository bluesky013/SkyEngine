//
// Aurora RDG pass builders.
//

#pragma once

#include <aurora/rdg/CompiledGraph.h>
#include <aurora/rdg/RDGHandles.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {

    class RenderGraph;
    class ResourceGroup;
    class GraphicsPipeline;
    class ComputePipeline;

    class SceneRasterPassBuilder {
    public:
        SceneRasterPassBuilder(RenderGraph *graph, uint32_t passIndex);

        SceneRasterPassBuilder &Read(RDGTextureHandle handle, AccessFlags access);
        SceneRasterPassBuilder &Read(RDGBufferHandle handle, AccessFlags access);
        SceneRasterPassBuilder &Write(RDGTextureHandle handle, AccessFlags access);
        SceneRasterPassBuilder &Write(RDGBufferHandle handle, AccessFlags access);

        SceneRasterPassBuilder &ColorAttachment(uint32_t slot, RDGTextureHandle handle, LoadOp loadOp, StoreOp storeOp);
        SceneRasterPassBuilder &DepthStencilAttachment(RDGTextureHandle handle,
                                                       LoadOp depthLoadOp, StoreOp depthStoreOp,
                                                       LoadOp stencilLoadOp, StoreOp stencilStoreOp);
        SceneRasterPassBuilder &AddDrawItem(const DrawItem &item);
        uint32_t AddQueue(const Name &name, QueueSortPolicy sortPolicy = QueueSortPolicy::NONE, const Name &tag = Name{});
        SceneRasterPassBuilder &AddDrawItem(uint32_t queue, const DrawItem &item);
        SceneRasterPassBuilder &SetQueueResourceGroup(uint32_t queue, ResourceGroup *group);
        SceneRasterPassBuilder &SetPassResourceGroup(ResourceGroup *group);

    private:
        RenderGraph *mGraph     = nullptr;
        uint32_t     mPassIndex = INVALID_INDEX;
    };

    class FullScreenPassBuilder {
    public:
        FullScreenPassBuilder(RenderGraph *graph, uint32_t passIndex);

        FullScreenPassBuilder &SetTechnique(GraphicsPipeline *pso);
        FullScreenPassBuilder &SetPassResourceGroup(ResourceGroup *group);
        FullScreenPassBuilder &SetTarget(RDGTextureHandle handle, LoadOp loadOp, StoreOp storeOp);
        FullScreenPassBuilder &SetDepthStencil(RDGTextureHandle handle,
                                               LoadOp depthLoadOp, StoreOp depthStoreOp,
                                               LoadOp stencilLoadOp, StoreOp stencilStoreOp);
        FullScreenPassBuilder &SetInputSRV(RDGTextureHandle handle);

    private:
        RenderGraph *mGraph     = nullptr;
        uint32_t     mPassIndex = INVALID_INDEX;
    };

    class ComputePassBuilder {
    public:
        ComputePassBuilder(RenderGraph *graph, uint32_t passIndex);

        ComputePassBuilder &Read(RDGTextureHandle handle, AccessFlags access);
        ComputePassBuilder &Read(RDGBufferHandle handle, AccessFlags access);
        ComputePassBuilder &Write(RDGTextureHandle handle, AccessFlags access);
        ComputePassBuilder &Write(RDGBufferHandle handle, AccessFlags access);

        ComputePassBuilder &SetPipeline(ComputePipeline *pso);
        ComputePassBuilder &SetPassResourceGroup(ResourceGroup *group);
        ComputePassBuilder &SetGroups(uint32_t x, uint32_t y, uint32_t z);

    private:
        RenderGraph *mGraph     = nullptr;
        uint32_t     mPassIndex = INVALID_INDEX;
    };

    class CopyBlitPassBuilder {
    public:
        CopyBlitPassBuilder(RenderGraph *graph, uint32_t passIndex);

        CopyBlitPassBuilder &Src(RDGTextureHandle handle);
        CopyBlitPassBuilder &Src(RDGBufferHandle handle);
        CopyBlitPassBuilder &Dst(RDGTextureHandle handle);
        CopyBlitPassBuilder &Dst(RDGBufferHandle handle);
        CopyBlitPassBuilder &SetKind(CopyBlitPayload::Kind kind);
        CopyBlitPassBuilder &SetSize(uint64_t size);
        CopyBlitPassBuilder &SetOffsets(uint64_t srcOffset, uint64_t dstOffset);

    private:
        RenderGraph *mGraph     = nullptr;
        uint32_t     mPassIndex = INVALID_INDEX;
    };

    class PresentPassBuilder {
    public:
        PresentPassBuilder(RenderGraph *graph, uint32_t passIndex);

        PresentPassBuilder &SetSource(RDGTextureHandle handle);

    private:
        RenderGraph *mGraph     = nullptr;
        uint32_t     mPassIndex = INVALID_INDEX;
    };

    class CustomPassBuilder {
    public:
        CustomPassBuilder(RenderGraph *graph, uint32_t passIndex);

        CustomPassBuilder &Read(RDGTextureHandle handle, AccessFlags access);
        CustomPassBuilder &Read(RDGBufferHandle handle, AccessFlags access);
        CustomPassBuilder &Write(RDGTextureHandle handle, AccessFlags access);
        CustomPassBuilder &Write(RDGBufferHandle handle, AccessFlags access);

    private:
        RenderGraph *mGraph     = nullptr;
        uint32_t     mPassIndex = INVALID_INDEX;
    };

} // namespace sky::aurora
