//
// Aurora RDG setup phase (shared, backend-agnostic).
//

#include <aurora/rdg/RenderGraph.h>
#include <aurora/rhi/Device.h>

#include "TransientPool.h"

#include <utility>

namespace sky::aurora {

    RenderGraph::RenderGraph(Device *device) : mDevice(device)
    {
        mPool = std::make_unique<ObjectPool>(device);
        mBackend.reset(device->CreateRDGBackend());
    }

    RenderGraph::~RenderGraph() = default;

    std::unique_ptr<RenderGraph> RenderGraph::Build(Device *device)
    {
        return std::make_unique<RenderGraph>(device);
    }

    const TransientPoolStats &RenderGraph::GetPoolStats() const
    {
        return mPool->GetStats();
    }

    uint32_t RenderGraph::AddResource(const Name &name, ResourceTag tag)
    {
        const uint32_t index = static_cast<uint32_t>(mResources.size());

        ResourceNode node;
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

        PassNode node;
        node.name = name;
        node.tag  = tag;

        if (std::holds_alternative<RasterPassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mRasterPasses.size());
            mRasterPasses.emplace_back();
        } else if (std::holds_alternative<ComputePassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mComputePasses.size());
            mComputePasses.emplace_back();
        } else if (std::holds_alternative<CopyPassTag>(tag)) {
            node.payloadIndex = static_cast<uint32_t>(mCopyPasses.size());
            mCopyPasses.emplace_back();
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

    void RenderGraph::AddRasterPass(const Name &name,
                                    const std::function<void(RasterPassBuilder &)> &setup,
                                    std::function<void(GraphicsEncoder &, RDGContext &)> execute)
    {
        const uint32_t passIndex = AddPass(name, RasterPassTag{});
        mRasterPasses[mPasses[passIndex].payloadIndex].executeFn = std::move(execute);

        RasterPassBuilder builder(this, passIndex);
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

    void RenderGraph::AddCopyPass(const Name &name,
                                  const std::function<void(CopyPassBuilder &)> &setup,
                                  std::function<void(BlitEncoder &, RDGContext &)> execute)
    {
        const uint32_t passIndex = AddPass(name, CopyPassTag{});
        mCopyPasses[mPasses[passIndex].payloadIndex].executeFn = std::move(execute);

        CopyPassBuilder builder(this, passIndex);
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
        auto &data = mRasterPasses[mPasses[passIndex].payloadIndex];

        RasterPassData::ColorAttachmentRef ref;
        ref.slot          = slot;
        ref.resourceIndex = resourceIndex;
        ref.loadOp        = loadOp;
        ref.storeOp       = storeOp;
        data.colors.push_back(std::move(ref));

        AddWrite(passIndex, resourceIndex, AccessFlagBit::RTV);
    }

    void RenderGraph::SetDepthStencilAttachment(uint32_t passIndex, uint32_t resourceIndex,
                                                LoadOp depthLoadOp, StoreOp depthStoreOp,
                                                LoadOp stencilLoadOp, StoreOp stencilStoreOp)
    {
        auto &data                  = mRasterPasses[mPasses[passIndex].payloadIndex];
        data.depthStencilResource  = resourceIndex;
        data.depthLoadOp           = depthLoadOp;
        data.depthStoreOp          = depthStoreOp;
        data.stencilLoadOp         = stencilLoadOp;
        data.stencilStoreOp        = stencilStoreOp;

        AddWrite(passIndex, resourceIndex, AccessFlagBit::DSV);
    }

    void RenderGraph::SetCopySrc(uint32_t passIndex, uint32_t resourceIndex)
    {
        AddRead(passIndex, resourceIndex, AccessFlagBit::COPY_SRC);
    }

    void RenderGraph::SetCopyDst(uint32_t passIndex, uint32_t resourceIndex)
    {
        AddWrite(passIndex, resourceIndex, AccessFlagBit::COPY_DST);
    }

    // ---- builders ----

    RasterPassBuilder::RasterPassBuilder(RenderGraph *graph, uint32_t passIndex)
        : mGraph(graph), mPassIndex(passIndex)
    {
    }

    RasterPassBuilder &RasterPassBuilder::Read(RDGTextureHandle handle, AccessFlags access)
    {
        mGraph->AddRead(mPassIndex, handle.id, access);
        return *this;
    }

    RasterPassBuilder &RasterPassBuilder::Read(RDGBufferHandle handle, AccessFlags access)
    {
        mGraph->AddRead(mPassIndex, handle.id, access);
        return *this;
    }

    RasterPassBuilder &RasterPassBuilder::Write(RDGTextureHandle handle, AccessFlags access)
    {
        mGraph->AddWrite(mPassIndex, handle.id, access);
        return *this;
    }

    RasterPassBuilder &RasterPassBuilder::Write(RDGBufferHandle handle, AccessFlags access)
    {
        mGraph->AddWrite(mPassIndex, handle.id, access);
        return *this;
    }

    RasterPassBuilder &RasterPassBuilder::ColorAttachment(uint32_t slot, RDGTextureHandle handle, LoadOp loadOp, StoreOp storeOp)
    {
        mGraph->SetColorAttachment(mPassIndex, slot, handle.id, loadOp, storeOp);
        return *this;
    }

    RasterPassBuilder &RasterPassBuilder::DepthStencilAttachment(RDGTextureHandle handle,
                                                                 LoadOp depthLoadOp, StoreOp depthStoreOp,
                                                                 LoadOp stencilLoadOp, StoreOp stencilStoreOp)
    {
        mGraph->SetDepthStencilAttachment(mPassIndex, handle.id, depthLoadOp, depthStoreOp, stencilLoadOp, stencilStoreOp);
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

    CopyPassBuilder::CopyPassBuilder(RenderGraph *graph, uint32_t passIndex)
        : mGraph(graph), mPassIndex(passIndex)
    {
    }

    CopyPassBuilder &CopyPassBuilder::Src(RDGTextureHandle handle)
    {
        mGraph->SetCopySrc(mPassIndex, handle.id);
        return *this;
    }

    CopyPassBuilder &CopyPassBuilder::Dst(RDGTextureHandle handle)
    {
        mGraph->SetCopyDst(mPassIndex, handle.id);
        return *this;
    }

} // namespace sky::aurora
