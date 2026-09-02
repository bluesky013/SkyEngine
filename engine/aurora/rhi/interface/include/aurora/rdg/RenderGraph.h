//
// Aurora RDG main class.
//
// RDG is an extension of the RHI interface: the graph structure, setup, and
// backend-agnostic analysis (deps / topo / lifetime / culling / transient) live
// here, while the backend-specific barrier compiler and pass executor live in a
// per-backend RDGBackend (obtained via Device::CreateRDGBackend()).
//
// When NOT to use RDG: for low-complexity scenes (demo / tool / clear + blit),
// driving Encoder directly and issuing PipelineBarrier by hand is still a valid
// path. RDG pays off once a frame has many passes with explicit data dependencies
// that benefit from automatic barrier derivation, transient aliasing, and culling.
//

#pragma once

#include <core/name/Name.h>

#include <aurora/rdg/RDGBackend.h>
#include <aurora/rdg/RDGContext.h>
#include <aurora/rdg/RDGGraph.h>
#include <aurora/rdg/RDGHandles.h>
#include <aurora/rdg/RDGTypes.h>
#include <aurora/rdg/RenderGraphBuilder.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Buffer.h>

#include <functional>
#include <memory>
#include <vector>

namespace sky::aurora {

    class Device;
    class CommandBuffer;
    class TransientPool;

    class RenderGraph {
    public:
        explicit RenderGraph(Device *device);
        ~RenderGraph();

        static std::unique_ptr<RenderGraph> Build(Device *device);

        // ---- resources ----
        RDGTextureHandle CreateTexture(const Name &name, const RDGTextureDesc &desc);
        RDGBufferHandle  CreateBuffer(const Name &name, const RDGBufferDesc &desc);
        RDGTextureHandle Import(const Name &name, const ImagePtr &image, AccessFlags importAccess = AccessFlagBit::NONE);
        RDGBufferHandle  Import(const Name &name, const BufferPtr &buffer, AccessFlags importAccess = AccessFlagBit::NONE);

        // ---- passes ----
        void AddRasterPass(const Name &name,
                           const std::function<void(RasterPassBuilder &)> &setup,
                           std::function<void(GraphicsEncoder &, RDGContext &)> execute);
        void AddComputePass(const Name &name,
                            const std::function<void(ComputePassBuilder &)> &setup,
                            std::function<void(ComputeEncoder &, RDGContext &)> execute);
        void AddCopyPass(const Name &name,
                         const std::function<void(CopyPassBuilder &)> &setup,
                         std::function<void(BlitEncoder &, RDGContext &)> execute);

        void MarkOfInterest(RDGTextureHandle handle);

        // ---- phases ----
        void Compile();
        void Execute(CommandBuffer *cmdBuf);

        // ---- internal (used by builders) ----
        void AddRead(uint32_t passIndex, uint32_t resourceIndex, AccessFlags access);
        void AddWrite(uint32_t passIndex, uint32_t resourceIndex, AccessFlags access);
        void SetColorAttachment(uint32_t passIndex, uint32_t slot, uint32_t resourceIndex, LoadOp loadOp, StoreOp storeOp);
        void SetDepthStencilAttachment(uint32_t passIndex, uint32_t resourceIndex,
                                       LoadOp depthLoadOp, StoreOp depthStoreOp,
                                       LoadOp stencilLoadOp, StoreOp stencilStoreOp);
        void SetCopySrc(uint32_t passIndex, uint32_t resourceIndex);
        void SetCopyDst(uint32_t passIndex, uint32_t resourceIndex);

        // ---- shared compiler / executor logic (backend reuses via RDGBackend) ----
        void DeriveBarriers();
        void ExecutePasses(CommandBuffer *cmdBuf);

        // ---- debug / test accessors ----
        const std::vector<PassNode>     &GetPasses() const { return mPasses; }
        const std::vector<ResourceNode> &GetResources() const { return mResources; }
        const std::vector<uint32_t>     &GetTopologicalOrder() const { return mTopoOrder; }
        const TransientPoolStats        &GetPoolStats() const;

    private:
        uint32_t AddResource(const Name &name, ResourceTag tag);
        uint32_t AddPass(const Name &name, PassTag tag);
        void AddDependency(uint32_t passIndex, uint32_t resourceIndex, AccessFlags access);

        // shared (backend-agnostic) compile steps
        void BuildDependencies();
        void TopologicalSort();
        void ComputeLifeTimes();
        void CullPasses();
        void BindTransientResources();

        // barrier helpers
        ImageSubRange FullSubRange(uint32_t resourceIndex) const;
        PipelineStageFlags StageForAccess(AccessFlags access, uint32_t passIndex) const;
        BarrierInfo MakeImageBarrier(Image *image, AccessFlags src, AccessFlags dst, const ImageSubRange &range,
                                     PipelineStageFlags srcStage, PipelineStageFlags dstStage) const;
        BarrierInfo MakeBufferBarrier(Buffer *buffer, AccessFlags src, AccessFlags dst,
                                      PipelineStageFlags srcStage, PipelineStageFlags dstStage) const;

        Device *mDevice = nullptr;
        std::unique_ptr<RDGBackend> mBackend;

        std::vector<ResourceNode>      mResources;
        std::vector<GraphImage>        mImages;
        std::vector<GraphImportImage>  mImportImages;
        std::vector<GraphBuffer>       mBuffers;
        std::vector<GraphImportBuffer> mImportBuffers;

        std::vector<PassNode>        mPasses;
        std::vector<RasterPassData>  mRasterPasses;
        std::vector<ComputePassData> mComputePasses;
        std::vector<CopyPassData>    mCopyPasses;

        std::vector<uint32_t> mTopoOrder;
        std::vector<uint32_t> mRank;
        std::vector<uint32_t> mOfInterest;
        std::vector<BarrierInfo> mFinalBarriers;

        std::vector<Image *>  mResolvedImages;
        std::vector<Buffer *> mResolvedBuffers;

        std::unique_ptr<TransientPool> mPool;

        bool mCompiled = false;
    };

} // namespace sky::aurora
