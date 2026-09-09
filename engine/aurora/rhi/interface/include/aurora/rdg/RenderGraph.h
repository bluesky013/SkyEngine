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
#include <core/memory/FrameAllocator.h>

#include <aurora/rdg/CompiledGraph.h>
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
        explicit RenderGraph(Device *device, FrameAllocator &frameAlloc);
        ~RenderGraph();

        static std::unique_ptr<RenderGraph> Build(Device *device, FrameAllocator &frameAlloc);

        // ---- resources ----
        RDGTextureHandle CreateTexture(const Name &name, const RDGTextureDesc &desc);
        RDGBufferHandle  CreateBuffer(const Name &name, const RDGBufferDesc &desc);
        RDGTextureHandle Import(const Name &name, const ImagePtr &image, AccessFlags importAccess = AccessFlagBit::NONE);
        RDGBufferHandle  Import(const Name &name, const BufferPtr &buffer, AccessFlags importAccess = AccessFlagBit::NONE);

        // ---- passes ----
        void AddSceneRasterPass(const Name &name,
                                const std::function<void(SceneRasterPassBuilder &)> &setup);
        void AddFullScreenPass(const Name &name,
                               const std::function<void(FullScreenPassBuilder &)> &setup);
        void AddComputePass(const Name &name,
                            const std::function<void(ComputePassBuilder &)> &setup,
                            std::function<void(ComputeEncoder &, RDGContext &)> execute = nullptr);
        void AddCopyBlitPass(const Name &name,
                             const std::function<void(CopyBlitPassBuilder &)> &setup);
        void AddPresentPass(const Name &name,
                            const std::function<void(PresentPassBuilder &)> &setup);
        void AddCustomPass(const Name &name,
                           const std::function<void(CustomPassBuilder &)> &setup,
                           std::function<void(RDGContext &, CommandBuffer &)> execute);

        void MarkOfInterest(RDGTextureHandle handle);

        // ---- phases ----
        void Compile();
        void Execute(CommandBuffer *cmdBuf);

        // ---- compiled output ----
        const CompiledGraph *GetCompiledGraph() const { return mCompiledGraph.get(); }

        // ---- internal (used by builders) ----
        void AddRead(uint32_t passIndex, uint32_t resourceIndex, AccessFlags access);
        void AddWrite(uint32_t passIndex, uint32_t resourceIndex, AccessFlags access);
        void SetColorAttachment(uint32_t passIndex, uint32_t slot, uint32_t resourceIndex, LoadOp loadOp, StoreOp storeOp);
        void SetDepthStencilAttachment(uint32_t passIndex, uint32_t resourceIndex,
                                       LoadOp depthLoadOp, StoreOp depthStoreOp,
                                       LoadOp stencilLoadOp, StoreOp stencilStoreOp);
        void SetCopySrc(uint32_t passIndex, uint32_t resourceIndex);
        void SetCopyDst(uint32_t passIndex, uint32_t resourceIndex);
        void AddDrawItem(uint32_t passIndex, const DrawItem &item);
        uint32_t AddQueue(uint32_t passIndex, const Name &name, QueueSortPolicy sortPolicy);
        void AddDrawItem(uint32_t passIndex, uint32_t queue, const DrawItem &item);
        void SetQueueResourceGroup(uint32_t passIndex, uint32_t queue, ResourceGroup *group);
        void SetSceneRasterResourceGroup(uint32_t passIndex, ResourceGroup *group);
        void SetFullScreenTechnique(uint32_t passIndex, GraphicsPipeline *pso);
        void SetFullScreenResourceGroup(uint32_t passIndex, ResourceGroup *group);
        void SetComputePipeline(uint32_t passIndex, ComputePipeline *pso);
        void SetComputeResourceGroup(uint32_t passIndex, ResourceGroup *group);
        void SetComputeGroups(uint32_t passIndex, uint32_t x, uint32_t y, uint32_t z);
        void SetCopyBlitKind(uint32_t passIndex, CopyBlitPayload::Kind kind);
        void SetCopyBlitSize(uint32_t passIndex, uint64_t size);
        void SetCopyBlitOffsets(uint32_t passIndex, uint64_t srcOffset, uint64_t dstOffset);

        // ---- shared compiler / executor logic (backend reuses via RDGBackend) ----
        void DeriveBarriers();
        void ExecutePasses(CommandBuffer *cmdBuf);

        // ---- debug / test accessors ----
        const TransientVector<PassNode>     &GetPasses() const { return mPasses; }
        const TransientVector<ResourceNode> &GetResources() const { return mResources; }
        const TransientVector<uint32_t>     &GetTopologicalOrder() const { return mTopoOrder; }
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
        void ProduceCompiledGraph();

        // barrier helpers
        ImageSubRange FullSubRange(uint32_t resourceIndex) const;
        PipelineStageFlags StageForAccess(AccessFlags access, uint32_t passIndex) const;
        BarrierInfo MakeImageBarrier(Image *image, AccessFlags src, AccessFlags dst, const ImageSubRange &range,
                                     PipelineStageFlags srcStage, PipelineStageFlags dstStage) const;
        BarrierInfo MakeBufferBarrier(Buffer *buffer, AccessFlags src, AccessFlags dst,
                                      PipelineStageFlags srcStage, PipelineStageFlags dstStage) const;

        Device *mDevice = nullptr;
        FrameAllocator *mFrameAlloc = nullptr;
        std::unique_ptr<RDGBackend> mBackend;

        TransientVector<ResourceNode>      mResources;
        TransientVector<GraphImage>        mImages;
        TransientVector<GraphImportImage>  mImportImages;
        TransientVector<GraphBuffer>       mBuffers;
        TransientVector<GraphImportBuffer> mImportBuffers;

        TransientVector<PassNode>            mPasses;
        TransientVector<SceneRasterPassData> mSceneRasterPasses;
        TransientVector<FullScreenPassData>  mFullScreenPasses;
        TransientVector<ComputePassData>     mComputePasses;
        TransientVector<CopyBlitPassData>    mCopyBlitPasses;
        TransientVector<PresentPassData>     mPresentPasses;
        TransientVector<CustomPassData>      mCustomPasses;

        TransientVector<uint32_t> mTopoOrder;
        TransientVector<uint32_t> mRank;
        TransientVector<uint32_t> mOfInterest;
        TransientVector<BarrierInfo> mFinalBarriers;

        TransientVector<Image *>  mResolvedImages;
        TransientVector<Buffer *> mResolvedBuffers;

        std::unique_ptr<TransientPool> mPool;
        std::unique_ptr<CompiledGraph> mCompiledGraph;

        bool mCompiled = false;
    };

} // namespace sky::aurora
