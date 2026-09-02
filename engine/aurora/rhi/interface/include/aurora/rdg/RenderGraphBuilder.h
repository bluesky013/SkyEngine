//
// Aurora RDG pass builders.
//

#pragma once

#include <aurora/rdg/RDGHandles.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {

    class RenderGraph;

    class RasterPassBuilder {
    public:
        RasterPassBuilder(RenderGraph *graph, uint32_t passIndex);

        RasterPassBuilder &Read(RDGTextureHandle handle, AccessFlags access);
        RasterPassBuilder &Read(RDGBufferHandle handle, AccessFlags access);
        RasterPassBuilder &Write(RDGTextureHandle handle, AccessFlags access);
        RasterPassBuilder &Write(RDGBufferHandle handle, AccessFlags access);

        RasterPassBuilder &ColorAttachment(uint32_t slot, RDGTextureHandle handle, LoadOp loadOp, StoreOp storeOp);
        RasterPassBuilder &DepthStencilAttachment(RDGTextureHandle handle,
                                                  LoadOp depthLoadOp, StoreOp depthStoreOp,
                                                  LoadOp stencilLoadOp, StoreOp stencilStoreOp);

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

    private:
        RenderGraph *mGraph     = nullptr;
        uint32_t     mPassIndex = INVALID_INDEX;
    };

    class CopyPassBuilder {
    public:
        CopyPassBuilder(RenderGraph *graph, uint32_t passIndex);

        CopyPassBuilder &Src(RDGTextureHandle handle);
        CopyPassBuilder &Dst(RDGTextureHandle handle);

    private:
        RenderGraph *mGraph     = nullptr;
        uint32_t     mPassIndex = INVALID_INDEX;
    };

} // namespace sky::aurora
