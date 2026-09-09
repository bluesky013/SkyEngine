//
// Aurora RDG setup phase (shared, backend-agnostic).
//

#include <aurora/rdg/RenderGraph.h>
#include <aurora/rhi/Device.h>

#include "TransientPool.h"

#include <utility>

namespace sky::aurora {

    RenderGraph::RenderGraph(Device *device, FrameAllocator &frameAlloc)
        : mDevice(device)
        , mFrameAlloc(&frameAlloc)
        , mResources(TransientStdAllocator<ResourceNode>{frameAlloc.Arena()})
        , mImages(TransientStdAllocator<GraphImage>{frameAlloc.Arena()})
        , mImportImages(TransientStdAllocator<GraphImportImage>{frameAlloc.Arena()})
        , mBuffers(TransientStdAllocator<GraphBuffer>{frameAlloc.Arena()})
        , mImportBuffers(TransientStdAllocator<GraphImportBuffer>{frameAlloc.Arena()})
        , mPasses(TransientStdAllocator<PassNode>{frameAlloc.Arena()})
        , mSceneRasterPasses(TransientStdAllocator<SceneRasterPassData>{frameAlloc.Arena()})
        , mFullScreenPasses(TransientStdAllocator<FullScreenPassData>{frameAlloc.Arena()})
        , mComputePasses(TransientStdAllocator<ComputePassData>{frameAlloc.Arena()})
        , mCopyBlitPasses(TransientStdAllocator<CopyBlitPassData>{frameAlloc.Arena()})
        , mPresentPasses(TransientStdAllocator<PresentPassData>{frameAlloc.Arena()})
        , mCustomPasses(TransientStdAllocator<CustomPassData>{frameAlloc.Arena()})
        , mTopoOrder(TransientStdAllocator<uint32_t>{frameAlloc.Arena()})
        , mRank(TransientStdAllocator<uint32_t>{frameAlloc.Arena()})
        , mOfInterest(TransientStdAllocator<uint32_t>{frameAlloc.Arena()})
        , mFinalBarriers(TransientStdAllocator<BarrierInfo>{frameAlloc.Arena()})
        , mResolvedImages(TransientStdAllocator<Image *>{frameAlloc.Arena()})
        , mResolvedBuffers(TransientStdAllocator<Buffer *>{frameAlloc.Arena()})
    {
        mPool = std::make_unique<ObjectPool>(device);
        mBackend.reset(device->CreateRDGBackend());
    }

    RenderGraph::~RenderGraph() = default;

    std::unique_ptr<RenderGraph> RenderGraph::Build(Device *device, FrameAllocator &frameAlloc)
    {
        return std::make_unique<RenderGraph>(device, frameAlloc);
    }

    const TransientPoolStats &RenderGraph::GetPoolStats() const
    {
        return mPool->GetStats();
    }

    uint32_t RenderGraph::AddResource(const Name &name, ResourceTag tag)
    {
        const uint32_t index = static_cast<uint32_t>(mResources.size());

        ResourceNode node(mFrameAlloc->Arena());
        node.name = name;
        node.tag  = tag;

        if (std::holds_alternative<TransientImageTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mImages.size());
            mImages.emplace_back();
        } else if (std::holds_alternative<ImportImageTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mImportImages.size());
            mImportImages.emplace_back();
        } else if (std::holds_alternative<TransientBufferTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mBuffers.size());
            mBuffers.emplace_back();
        } else if (std::holds_alternative<ImportBufferTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mImportBuffers.size());
            mImportBuffers.emplace_back();
        }

        mResources.push_back(std::move(node));
        mResolvedImages.push_back(nullptr);
        mResolvedBuffers.push_back(nullptr);
        return index;
    }

    uint32_t RenderGraph::AddPass(const Name &name, PassTag tag)
    {
        const uint32_t index = static_cast<uint32_t>(mPasses.size());

        PassNode node(mFrameAlloc->Arena());
        node.name = name;
        node.tag  = tag;

        if (std::holds_alternative<SceneRasterPassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mSceneRasterPasses.size());
            mSceneRasterPasses.emplace_back(mFrameAlloc->Arena());
        } else if (std::holds_alternative<FullScreenPassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mFullScreenPasses.size());
            mFullScreenPasses.emplace_back(mFrameAlloc->Arena());
        } else if (std::holds_alternative<ComputePassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mComputePasses.size());
            mComputePasses.emplace_back();
        } else if (std::holds_alternative<CopyBlitPassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mCopyBlitPasses.size());
            mCopyBlitPasses.emplace_back();
        } else if (std::holds_alternative<PresentPassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mPresentPasses.size());
            mPresentPasses.emplace_back();
        } else if (std::holds_alternative<CustomPassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mCustomPasses.size());
            mCustomPasses.emplace_back();
        }

        mPasses.push_back(std::move(node));
        return index;
    }

    RDGTextureHandle RenderGraph::CreateTexture(const Name &name, const RDGTextureDesc &desc)
    {
        const uint32_t index = AddResource(name, TransientImageTag{});
        mImages[mResources[index].payloadIndex].desc = desc;
        return RDGTextureHandle{index};
    }

    RDGBufferHandle RenderGraph::CreateBuffer(const Name &name, const RDGBufferDesc &desc)
    {
        const uint32_t index = AddResource(name, TransientBufferTag{});
        mBuffers[mResources[index].payloadIndex].desc = desc;
        return RDGBufferHandle{index};
    }

    RDGTextureHandle RenderGraph::Import(const Name &name, const ImagePtr &image, AccessFlags importAccess)
    {
        const uint32_t index = AddResource(name, ImportImageTag{});
        auto &import          = mImportImages[mResources[index].payloadIndex];
        import.image          = image;
        import.importAccess   = importAccess;
        return RDGTextureHandle{index};
    }

    RDGBufferHandle RenderGraph::Import(const Name &name, const BufferPtr &buffer, AccessFlags importAccess)
    {
        const uint32_t index = AddResource(name, ImportBufferTag{});
        auto &import          = mImportBuffers[mResources[index].payloadIndex];
        import.buffer         = buffer;
        import.importAccess   = importAccess;
        return RDGBufferHandle{index};
    }

    void RenderGraph::AddSceneRasterPass(const Name &name,
                                         const std::function<void(SceneRasterPassBuilder &)> &setup)
    {
        const uint32_t passIndex = AddPass(name, SceneRasterPassTag{});

        SceneRasterPassBuilder builder(this, passIndex);
        if (setup) {
            setup(builder);
        }
    }

    void RenderGraph::AddFullScreenPass(const Name &name,
                                        const std::function<void(FullScreenPassBuilder &)> &setup)
    {
        const uint32_t passIndex = AddPass(name, FullScreenPassTag{});

        FullScreenPassBuilder builder(this, passIndex);
        if (setup) {
            setup(builder);
        }
    }

    void RenderGraph::AddComputePass(const Name &name,
                                     const std::function<void(ComputePassBuilder &)> &setup,
                                     std::function<void(ComputeEncoder &, RDGContext &)> execute)
    {
        const uint32_t passIndex = AddPass(name, ComputePassTag{});
        mComputePasses[mPasses[passIndex].payloadIndex].executeFn = std::move(execute);

        ComputePassBuilder builder(this, passIndex);
        if (setup) {
            setup(builder);
        }
    }

    void RenderGraph::AddCopyBlitPass(const Name &name,
                                      const std::function<void(CopyBlitPassBuilder &)> &setup)
    {
        const uint32_t passIndex = AddPass(name, CopyBlitPassTag{});

        CopyBlitPassBuilder builder(this, passIndex);
        if (setup) {
            setup(builder);
        }
    }

    void RenderGraph::AddPresentPass(const Name &name,
                                     const std::function<void(PresentPassBuilder &)> &setup)
    {
        const uint32_t passIndex = AddPass(name, PresentPassTag{});

        PresentPassBuilder builder(this, passIndex);
        if (setup) {
            setup(builder);
        }
    }

    void RenderGraph::AddCustomPass(const Name &name,
                                    const std::function<void(CustomPassBuilder &)> &setup,
                                    std::function<void(RDGContext &, CommandBuffer &)> execute)
    {
        const uint32_t passIndex = AddPass(name, CustomPassTag{});
        mCustomPasses[mPasses[passIndex].payloadIndex].fn = std::move(execute);

        CustomPassBuilder builder(this, passIndex);
        if (setup) {
            setup(builder);
        }
    }

    void RenderGraph::MarkOfInterest(RDGTextureHandle handle)
    {
        if (handle.IsValid()) {
            mOfInterest.push_back(handle.id);
        }
    }

    void RenderGraph::AddRead(uint32_t passIndex, uint32_t resourceIndex, AccessFlags access)
    {
        if (resourceIndex >= mResources.size()) {
            return;
        }
        mPasses[passIndex].readResources.push_back(resourceIndex);
        AddDependency(passIndex, resourceIndex, access);
    }

    void RenderGraph::AddWrite(uint32_t passIndex, uint32_t resourceIndex, AccessFlags access)
    {
        if (resourceIndex >= mResources.size()) {
            return;
        }
        mPasses[passIndex].writeResources.push_back(resourceIndex);
        mResources[resourceIndex].lastWriterPass = passIndex;
        AddDependency(passIndex, resourceIndex, access);
    }

    void RenderGraph::AddDependency(uint32_t passIndex, uint32_t resourceIndex, AccessFlags access)
    {
        auto &node = mResources[resourceIndex];
        if (!node.accesses.empty()) {
            const auto &last = node.accesses.back();
            if (last.pass == passIndex && last.access == access) {
                return;
            }
        }
        node.accesses.push_back(AccessRecord{passIndex, access});
    }

    void RenderGraph::SetColorAttachment(uint32_t passIndex, uint32_t slot, uint32_t resourceIndex, LoadOp loadOp, StoreOp storeOp)
    {
        auto &pass = mPasses[passIndex];

        if (std::holds_alternative<SceneRasterPassTag>(pass.tag)) {
            auto &data = mSceneRasterPasses[pass.payloadIndex];
            SceneRasterPassData::ColorAttachmentRef ref;
            ref.slot          = slot;
            ref.resourceIndex = resourceIndex;
            ref.loadOp        = loadOp;
            ref.storeOp       = storeOp;
            data.colors.push_back(std::move(ref));
        } else if (std::holds_alternative<FullScreenPassTag>(pass.tag)) {
            auto &data = mFullScreenPasses[pass.payloadIndex];
            SceneRasterPassData::ColorAttachmentRef ref;
            ref.slot          = slot;
            ref.resourceIndex = resourceIndex;
            ref.loadOp        = loadOp;
            ref.storeOp       = storeOp;
            data.colors.push_back(std::move(ref));
        }

        AddWrite(passIndex, resourceIndex, AccessFlagBit::RTV);
    }

    void RenderGraph::SetDepthStencilAttachment(uint32_t passIndex, uint32_t resourceIndex,
                                                LoadOp depthLoadOp, StoreOp depthStoreOp,
                                                LoadOp stencilLoadOp, StoreOp stencilStoreOp)
    {
        auto &pass = mPasses[passIndex];

        if (std::holds_alternative<SceneRasterPassTag>(pass.tag)) {
            auto &data                 = mSceneRasterPasses[pass.payloadIndex];
            data.depthStencilResource  = resourceIndex;
            data.depthLoadOp           = depthLoadOp;
            data.depthStoreOp          = depthStoreOp;
            data.stencilLoadOp         = stencilLoadOp;
            data.stencilStoreOp        = stencilStoreOp;
        } else if (std::holds_alternative<FullScreenPassTag>(pass.tag)) {
            auto &data                 = mFullScreenPasses[pass.payloadIndex];
            data.depthStencilResource  = resourceIndex;
            data.depthLoadOp           = depthLoadOp;
            data.depthStoreOp          = depthStoreOp;
            data.stencilLoadOp         = stencilLoadOp;
            data.stencilStoreOp        = stencilStoreOp;
        }

        AddWrite(passIndex, resourceIndex, AccessFlagBit::DSV);
    }

    void RenderGraph::SetCopySrc(uint32_t passIndex, uint32_t resourceIndex)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<CopyBlitPassTag>(pass.tag)) {
            mCopyBlitPasses[pass.payloadIndex].srcResourceIndex = resourceIndex;
        }
        AddRead(passIndex, resourceIndex, AccessFlagBit::COPY_SRC);
    }

    void RenderGraph::SetCopyDst(uint32_t passIndex, uint32_t resourceIndex)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<CopyBlitPassTag>(pass.tag)) {
            mCopyBlitPasses[pass.payloadIndex].dstResourceIndex = resourceIndex;
        }
        AddWrite(passIndex, resourceIndex, AccessFlagBit::COPY_DST);
    }

    void RenderGraph::AddDrawItem(uint32_t passIndex, const DrawItem &item)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<SceneRasterPassTag>(pass.tag)) {
            auto &data = mSceneRasterPasses[pass.payloadIndex];
            if (data.queues.empty()) {
                data.queues.emplace_back(mFrameAlloc->Arena()).name = Name("default");
            }
            data.queues[0].items.push_back(item);
        }
    }

    uint32_t RenderGraph::AddQueue(uint32_t passIndex, const Name &name, QueueSortPolicy sortPolicy)
    {
        auto &pass = mPasses[passIndex];
        if (!std::holds_alternative<SceneRasterPassTag>(pass.tag)) {
            return INVALID_INDEX;
        }
        auto &data = mSceneRasterPasses[pass.payloadIndex];
        const uint32_t queueIndex = static_cast<uint32_t>(data.queues.size());
        auto &queue = data.queues.emplace_back(mFrameAlloc->Arena());
        queue.name       = name;
        queue.sortPolicy = sortPolicy;
        return queueIndex;
    }

    void RenderGraph::AddDrawItem(uint32_t passIndex, uint32_t queue, const DrawItem &item)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<SceneRasterPassTag>(pass.tag)) {
            auto &data = mSceneRasterPasses[pass.payloadIndex];
            if (queue < data.queues.size()) {
                data.queues[queue].items.push_back(item);
            }
        }
    }

    void RenderGraph::SetQueueResourceGroup(uint32_t passIndex, uint32_t queue, ResourceGroup *group)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<SceneRasterPassTag>(pass.tag)) {
            auto &data = mSceneRasterPasses[pass.payloadIndex];
            if (queue < data.queues.size()) {
                data.queues[queue].queueResourceGroup = group;
            }
        }
    }

    void RenderGraph::SetSceneRasterResourceGroup(uint32_t passIndex, ResourceGroup *group)
    {
        (void)passIndex; (void)group; // reserved for compiled payload wiring
    }

    void RenderGraph::SetFullScreenTechnique(uint32_t passIndex, GraphicsPipeline *pso)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<FullScreenPassTag>(pass.tag)) {
            mFullScreenPasses[pass.payloadIndex].pso = pso;
        }
    }

    void RenderGraph::SetFullScreenResourceGroup(uint32_t passIndex, ResourceGroup *group)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<FullScreenPassTag>(pass.tag)) {
            mFullScreenPasses[pass.payloadIndex].passResourceGroup = group;
        }
    }

    void RenderGraph::SetComputePipeline(uint32_t passIndex, ComputePipeline *pso)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<ComputePassTag>(pass.tag)) {
            mComputePasses[pass.payloadIndex].pso = pso;
        }
    }

    void RenderGraph::SetComputeResourceGroup(uint32_t passIndex, ResourceGroup *group)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<ComputePassTag>(pass.tag)) {
            mComputePasses[pass.payloadIndex].passResourceGroup = group;
        }
    }

    void RenderGraph::SetComputeGroups(uint32_t passIndex, uint32_t x, uint32_t y, uint32_t z)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<ComputePassTag>(pass.tag)) {
            auto &data   = mComputePasses[pass.payloadIndex];
            data.groupX  = x;
            data.groupY  = y;
            data.groupZ  = z;
        }
    }

    void RenderGraph::SetCopyBlitKind(uint32_t passIndex, CopyBlitPayload::Kind kind)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<CopyBlitPassTag>(pass.tag)) {
            mCopyBlitPasses[pass.payloadIndex].kind = kind;
        }
    }

    void RenderGraph::SetCopyBlitSize(uint32_t passIndex, uint64_t size)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<CopyBlitPassTag>(pass.tag)) {
            mCopyBlitPasses[pass.payloadIndex].size = size;
        }
    }

    void RenderGraph::SetCopyBlitOffsets(uint32_t passIndex, uint64_t srcOffset, uint64_t dstOffset)
    {
        auto &pass = mPasses[passIndex];
        if (std::holds_alternative<CopyBlitPassTag>(pass.tag)) {
            auto &data      = mCopyBlitPasses[pass.payloadIndex];
            data.srcOffset  = srcOffset;
            data.dstOffset  = dstOffset;
        }
    }

    // ---- builders ----

    SceneRasterPassBuilder::SceneRasterPassBuilder(RenderGraph *graph, uint32_t passIndex)
        : mGraph(graph), mPassIndex(passIndex)
    {
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::Read(RDGTextureHandle handle, AccessFlags access)
    {
        mGraph->AddRead(mPassIndex, handle.id, access);
        return *this;
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::Read(RDGBufferHandle handle, AccessFlags access)
    {
        mGraph->AddRead(mPassIndex, handle.id, access);
        return *this;
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::Write(RDGTextureHandle handle, AccessFlags access)
    {
        mGraph->AddWrite(mPassIndex, handle.id, access);
        return *this;
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::Write(RDGBufferHandle handle, AccessFlags access)
    {
        mGraph->AddWrite(mPassIndex, handle.id, access);
        return *this;
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::ColorAttachment(uint32_t slot, RDGTextureHandle handle, LoadOp loadOp, StoreOp storeOp)
    {
        mGraph->SetColorAttachment(mPassIndex, slot, handle.id, loadOp, storeOp);
        return *this;
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::DepthStencilAttachment(RDGTextureHandle handle,
                                                                           LoadOp depthLoadOp, StoreOp depthStoreOp,
                                                                           LoadOp stencilLoadOp, StoreOp stencilStoreOp)
    {
        mGraph->SetDepthStencilAttachment(mPassIndex, handle.id, depthLoadOp, depthStoreOp, stencilLoadOp, stencilStoreOp);
        return *this;
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::AddDrawItem(const DrawItem &item)
    {
        mGraph->AddDrawItem(mPassIndex, item);
        return *this;
    }

    uint32_t SceneRasterPassBuilder::AddQueue(const Name &name, QueueSortPolicy sortPolicy)
    {
        return mGraph->AddQueue(mPassIndex, name, sortPolicy);
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::AddDrawItem(uint32_t queue, const DrawItem &item)
    {
        mGraph->AddDrawItem(mPassIndex, queue, item);
        return *this;
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::SetQueueResourceGroup(uint32_t queue, ResourceGroup *group)
    {
        mGraph->SetQueueResourceGroup(mPassIndex, queue, group);
        return *this;
    }

    SceneRasterPassBuilder &SceneRasterPassBuilder::SetPassResourceGroup(ResourceGroup *group)
    {
        mGraph->SetSceneRasterResourceGroup(mPassIndex, group);
        return *this;
    }

    FullScreenPassBuilder::FullScreenPassBuilder(RenderGraph *graph, uint32_t passIndex)
        : mGraph(graph), mPassIndex(passIndex)
    {
    }

    FullScreenPassBuilder &FullScreenPassBuilder::SetTechnique(GraphicsPipeline *pso)
    {
        mGraph->SetFullScreenTechnique(mPassIndex, pso);
        return *this;
    }

    FullScreenPassBuilder &FullScreenPassBuilder::SetPassResourceGroup(ResourceGroup *group)
    {
        mGraph->SetFullScreenResourceGroup(mPassIndex, group);
        return *this;
    }

    FullScreenPassBuilder &FullScreenPassBuilder::SetTarget(RDGTextureHandle handle, LoadOp loadOp, StoreOp storeOp)
    {
        mGraph->SetColorAttachment(mPassIndex, 0, handle.id, loadOp, storeOp);
        return *this;
    }

    FullScreenPassBuilder &FullScreenPassBuilder::SetDepthStencil(RDGTextureHandle handle,
                                                                  LoadOp depthLoadOp, StoreOp depthStoreOp,
                                                                  LoadOp stencilLoadOp, StoreOp stencilStoreOp)
    {
        mGraph->SetDepthStencilAttachment(mPassIndex, handle.id, depthLoadOp, depthStoreOp, stencilLoadOp, stencilStoreOp);
        return *this;
    }

    FullScreenPassBuilder &FullScreenPassBuilder::SetInputSRV(RDGTextureHandle handle)
    {
        mGraph->AddRead(mPassIndex, handle.id, AccessFlagBit::SRV);
        return *this;
    }

    ComputePassBuilder::ComputePassBuilder(RenderGraph *graph, uint32_t passIndex)
        : mGraph(graph), mPassIndex(passIndex)
    {
    }

    ComputePassBuilder &ComputePassBuilder::Read(RDGTextureHandle handle, AccessFlags access)
    {
        mGraph->AddRead(mPassIndex, handle.id, access);
        return *this;
    }

    ComputePassBuilder &ComputePassBuilder::Read(RDGBufferHandle handle, AccessFlags access)
    {
        mGraph->AddRead(mPassIndex, handle.id, access);
        return *this;
    }

    ComputePassBuilder &ComputePassBuilder::Write(RDGTextureHandle handle, AccessFlags access)
    {
        mGraph->AddWrite(mPassIndex, handle.id, access);
        return *this;
    }

    ComputePassBuilder &ComputePassBuilder::Write(RDGBufferHandle handle, AccessFlags access)
    {
        mGraph->AddWrite(mPassIndex, handle.id, access);
        return *this;
    }

    ComputePassBuilder &ComputePassBuilder::SetPipeline(ComputePipeline *pso)
    {
        mGraph->SetComputePipeline(mPassIndex, pso);
        return *this;
    }

    ComputePassBuilder &ComputePassBuilder::SetPassResourceGroup(ResourceGroup *group)
    {
        mGraph->SetComputeResourceGroup(mPassIndex, group);
        return *this;
    }

    ComputePassBuilder &ComputePassBuilder::SetGroups(uint32_t x, uint32_t y, uint32_t z)
    {
        mGraph->SetComputeGroups(mPassIndex, x, y, z);
        return *this;
    }

    CopyBlitPassBuilder::CopyBlitPassBuilder(RenderGraph *graph, uint32_t passIndex)
        : mGraph(graph), mPassIndex(passIndex)
    {
    }

    CopyBlitPassBuilder &CopyBlitPassBuilder::Src(RDGTextureHandle handle)
    {
        mGraph->SetCopySrc(mPassIndex, handle.id);
        return *this;
    }

    CopyBlitPassBuilder &CopyBlitPassBuilder::Src(RDGBufferHandle handle)
    {
        mGraph->SetCopySrc(mPassIndex, handle.id);
        return *this;
    }

    CopyBlitPassBuilder &CopyBlitPassBuilder::Dst(RDGTextureHandle handle)
    {
        mGraph->SetCopyDst(mPassIndex, handle.id);
        return *this;
    }

    CopyBlitPassBuilder &CopyBlitPassBuilder::Dst(RDGBufferHandle handle)
    {
        mGraph->SetCopyDst(mPassIndex, handle.id);
        return *this;
    }

    CopyBlitPassBuilder &CopyBlitPassBuilder::SetKind(CopyBlitPayload::Kind kind)
    {
        mGraph->SetCopyBlitKind(mPassIndex, kind);
        return *this;
    }

    CopyBlitPassBuilder &CopyBlitPassBuilder::SetSize(uint64_t size)
    {
        mGraph->SetCopyBlitSize(mPassIndex, size);
        return *this;
    }

    CopyBlitPassBuilder &CopyBlitPassBuilder::SetOffsets(uint64_t srcOffset, uint64_t dstOffset)
    {
        mGraph->SetCopyBlitOffsets(mPassIndex, srcOffset, dstOffset);
        return *this;
    }

    PresentPassBuilder::PresentPassBuilder(RenderGraph *graph, uint32_t passIndex)
        : mGraph(graph), mPassIndex(passIndex)
    {
    }

    PresentPassBuilder &PresentPassBuilder::SetSource(RDGTextureHandle handle)
    {
        mGraph->AddRead(mPassIndex, handle.id, AccessFlagBit::PRESENT);
        return *this;
    }

    CustomPassBuilder::CustomPassBuilder(RenderGraph *graph, uint32_t passIndex)
        : mGraph(graph), mPassIndex(passIndex)
    {
    }

    CustomPassBuilder &CustomPassBuilder::Read(RDGTextureHandle handle, AccessFlags access)
    {
        mGraph->AddRead(mPassIndex, handle.id, access);
        return *this;
    }

    CustomPassBuilder &CustomPassBuilder::Read(RDGBufferHandle handle, AccessFlags access)
    {
        mGraph->AddRead(mPassIndex, handle.id, access);
        return *this;
    }

    CustomPassBuilder &CustomPassBuilder::Write(RDGTextureHandle handle, AccessFlags access)
    {
        mGraph->AddWrite(mPassIndex, handle.id, access);
        return *this;
    }

    CustomPassBuilder &CustomPassBuilder::Write(RDGBufferHandle handle, AccessFlags access)
    {
        mGraph->AddWrite(mPassIndex, handle.id, access);
        return *this;
    }

} // namespace sky::aurora
