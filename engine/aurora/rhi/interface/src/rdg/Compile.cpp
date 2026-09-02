//
// Aurora RDG compile phase (shared, backend-agnostic analysis).
//

#include <aurora/rdg/RenderGraph.h>
#include <aurora/rhi/Barrier.h>
#include <core/logger/Logger.h>

#include "TransientPool.h"

#include <algorithm>
#include <queue>
#include <utility>

namespace sky::aurora {

    static const char *TAG = "AuroraRDG";

    namespace {

        Image::Descriptor ToImageDesc(const RDGTextureDesc &desc)
        {
            Image::Descriptor d{};
            d.imageType   = ImageType::IMAGE_2D;
            d.format      = desc.format;
            d.extent      = desc.extent;
            d.mipLevels   = desc.mipLevels;
            d.arrayLayers = desc.arrayLayers;
            d.samples     = desc.samples;
            d.usage       = desc.usage;
            d.memory      = MemoryType::GPU_ONLY;
            return d;
        }

        Buffer::Descriptor ToBufferDesc(const RDGBufferDesc &desc)
        {
            Buffer::Descriptor d{};
            d.size   = desc.size;
            d.usage  = desc.usage;
            d.memory = MemoryType::GPU_ONLY;
            return d;
        }

    } // namespace

    void RenderGraph::Compile()
    {
        // reset compile-derived state so Compile can be re-run on the same graph
        for (auto &pass : mPasses) {
            pass.dependsOn.clear();
            pass.inDegree = 0;
            pass.live     = false;
            pass.frontBarriers.clear();
        }
        mTopoOrder.clear();
        mRank.clear();
        mFinalBarriers.clear();
        std::fill(mResolvedImages.begin(), mResolvedImages.end(), nullptr);
        std::fill(mResolvedBuffers.begin(), mResolvedBuffers.end(), nullptr);

        BuildDependencies();
        TopologicalSort();
        ComputeLifeTimes();
        CullPasses();
        BindTransientResources();
        mBackend->CompileBarriers(*this);

        // debug logger (live/culled + pool hit/miss)
        uint32_t liveCount = 0;
        for (const auto &pass : mPasses) {
            if (pass.live) {
                ++liveCount;
            }
        }
        const auto &stats = mPool->GetStats();
        LOG_I(TAG, "RDG compile: %u/%zu passes live, image hit=%u miss=%u, buffer hit=%u miss=%u",
              liveCount, mPasses.size(), stats.imageHits, stats.imageMisses, stats.bufferHits, stats.bufferMisses);

        mCompiled = true;
    }

    void RenderGraph::BuildDependencies()
    {
        for (const auto &node : mResources) {
            const auto &accesses = node.accesses;
            for (size_t i = 0; i + 1 < accesses.size(); ++i) {
                const uint32_t prevPass = accesses[i].pass;
                const uint32_t nextPass = accesses[i + 1].pass;
                if (prevPass == nextPass) {
                    continue;
                }

                auto &deps = mPasses[nextPass].dependsOn;
                if (std::find(deps.begin(), deps.end(), prevPass) == deps.end()) {
                    deps.push_back(prevPass);
                }
            }
        }
    }

    void RenderGraph::TopologicalSort()
    {
        mTopoOrder.clear();
        mRank.assign(mPasses.size(), 0);

        for (auto &pass : mPasses) {
            pass.inDegree = static_cast<uint32_t>(pass.dependsOn.size());
        }

        std::queue<uint32_t> ready;
        for (uint32_t i = 0; i < mPasses.size(); ++i) {
            if (mPasses[i].inDegree == 0) {
                ready.push(i);
            }
        }

        while (!ready.empty()) {
            const uint32_t passIndex = ready.front();
            ready.pop();
            mTopoOrder.push_back(passIndex);

            for (uint32_t i = 0; i < mPasses.size(); ++i) {
                auto &deps = mPasses[i].dependsOn;
                if (std::find(deps.begin(), deps.end(), passIndex) != deps.end()) {
                    if (--mPasses[i].inDegree == 0) {
                        ready.push(i);
                    }
                }
            }
        }

        for (uint32_t i = 0; i < mTopoOrder.size(); ++i) {
            mRank[mTopoOrder[i]] = i;
        }
    }

    void RenderGraph::ComputeLifeTimes()
    {
        for (auto &node : mResources) {
            if (node.accesses.empty()) {
                node.lifeTime.firstUsePass = 0;
                node.lifeTime.lastUsePass  = 0;
                continue;
            }

            uint32_t first = INVALID_INDEX;
            uint32_t last  = 0;
            for (const auto &rec : node.accesses) {
                const uint32_t rank = mRank[rec.pass];
                first = std::min(first, rank);
                last  = std::max(last, rank);
            }
            node.lifeTime.firstUsePass = first;
            node.lifeTime.lastUsePass  = last;
        }
    }

    void RenderGraph::CullPasses()
    {
        std::vector<bool> live(mPasses.size(), false);
        std::queue<uint32_t> work;

        const auto markLive = [&](uint32_t passIndex) {
            if (passIndex != INVALID_INDEX && passIndex < mPasses.size() && !live[passIndex]) {
                live[passIndex] = true;
                work.push(passIndex);
            }
        };

        for (uint32_t i = 0; i < mResources.size(); ++i) {
            const auto &node = mResources[i];
            bool isSeed = false;
            if (std::holds_alternative<ImportImageTag>(node.tag) || std::holds_alternative<ImportBufferTag>(node.tag)) {
                isSeed = true;
            }
            if (std::find(mOfInterest.begin(), mOfInterest.end(), i) != mOfInterest.end()) {
                isSeed = true;
            }
            if (isSeed) {
                markLive(node.lastWriterPass);
            }
        }

        while (!work.empty()) {
            const uint32_t passIndex = work.front();
            work.pop();

            const auto &pass = mPasses[passIndex];
            for (const uint32_t resIndex : pass.readResources) {
                markLive(mResources[resIndex].lastWriterPass);
            }
        }

        for (uint32_t i = 0; i < mPasses.size(); ++i) {
            mPasses[i].live = live[i];
        }
    }

    void RenderGraph::BindTransientResources()
    {
        // bind external (import) resources first
        for (uint32_t i = 0; i < mResources.size(); ++i) {
            const auto &node = mResources[i];
            if (std::holds_alternative<ImportImageTag>(node.tag)) {
                mResolvedImages[i] = mImportImages[node.payloadIndex].image;
            } else if (std::holds_alternative<ImportBufferTag>(node.tag)) {
                mResolvedBuffers[i] = mImportBuffers[node.payloadIndex].buffer;
            }
        }

        struct Item {
            uint32_t          resourceIndex;
            uint32_t          firstUse;
            uint32_t          lastUse;
            bool              isImage;
            Image::Descriptor imageDesc;
            Buffer::Descriptor bufferDesc;
        };

        std::vector<Item> items;
        for (uint32_t i = 0; i < mResources.size(); ++i) {
            const auto &node = mResources[i];

            bool usedByLivePass = false;
            for (const auto &rec : node.accesses) {
                if (mPasses[rec.pass].live) {
                    usedByLivePass = true;
                    break;
                }
            }
            if (!usedByLivePass) {
                continue;
            }

            if (std::holds_alternative<TransientImageTag>(node.tag)) {
                Item item;
                item.resourceIndex = i;
                item.firstUse      = node.lifeTime.firstUsePass;
                item.lastUse       = node.lifeTime.lastUsePass;
                item.isImage       = true;
                item.imageDesc     = ToImageDesc(mImages[node.payloadIndex].desc);
                items.push_back(item);
            } else if (std::holds_alternative<TransientBufferTag>(node.tag)) {
                Item item;
                item.resourceIndex = i;
                item.firstUse      = node.lifeTime.firstUsePass;
                item.lastUse       = node.lifeTime.lastUsePass;
                item.isImage       = false;
                item.bufferDesc    = ToBufferDesc(mBuffers[node.payloadIndex].desc);
                items.push_back(item);
            }
        }

        std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
            return a.firstUse < b.firstUse;
        });

        std::vector<std::pair<uint32_t, Image *>>  inflightImages;
        std::vector<std::pair<uint32_t, Buffer *>> inflightBuffers;

        for (const auto &item : items) {
            for (auto it = inflightImages.begin(); it != inflightImages.end();) {
                if (it->first < item.firstUse) {
                    mPool->ReleaseImage(it->second);
                    it = inflightImages.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = inflightBuffers.begin(); it != inflightBuffers.end();) {
                if (it->first < item.firstUse) {
                    mPool->ReleaseBuffer(it->second);
                    it = inflightBuffers.erase(it);
                } else {
                    ++it;
                }
            }

            if (item.isImage) {
                Image *image = mPool->AcquireImage(item.imageDesc);
                mResolvedImages[item.resourceIndex] = image;
                inflightImages.emplace_back(item.lastUse, image);
            } else {
                Buffer *buffer = mPool->AcquireBuffer(item.bufferDesc);
                mResolvedBuffers[item.resourceIndex] = buffer;
                inflightBuffers.emplace_back(item.lastUse, buffer);
            }
        }

        for (auto &entry : inflightImages) {
            mPool->ReleaseImage(entry.second);
        }
        for (auto &entry : inflightBuffers) {
            mPool->ReleaseBuffer(entry.second);
        }

        // resolve raster pass render areas from the first color attachment's extent
        for (uint32_t i = 0; i < mPasses.size(); ++i) {
            if (!mPasses[i].live || !std::holds_alternative<RasterPassTag>(mPasses[i].tag)) {
                continue;
            }
            auto &data = mRasterPasses[mPasses[i].payloadIndex];
            if (data.colors.empty()) {
                continue;
            }
            const uint32_t resIndex = data.colors.front().resourceIndex;
            if (resIndex >= mResources.size()) {
                continue;
            }
            const auto &node = mResources[resIndex];
            if (std::holds_alternative<TransientImageTag>(node.tag)) {
                const auto &desc = mImages[node.payloadIndex].desc;
                data.renderArea = {desc.extent.width, desc.extent.height};
            }
        }
    }

    ImageSubRange RenderGraph::FullSubRange(uint32_t resourceIndex) const
    {
        ImageSubRange range{};
        const auto &node = mResources[resourceIndex];
        if (std::holds_alternative<TransientImageTag>(node.tag)) {
            const auto &desc = mImages[node.payloadIndex].desc;
            range.baseLevel = 0;
            range.levels    = desc.mipLevels;
            range.baseLayer = 0;
            range.layers    = desc.arrayLayers;
            const auto &info = GetImageFormatInfo(desc.format);
            range.aspectMask = (info.hasDepth || info.hasStencil)
                                   ? (AspectFlagBit::DEPTH_BIT | AspectFlagBit::STENCIL_BIT)
                                   : AspectFlagBit::COLOR_BIT;
        } else {
            range.baseLevel  = 0;
            range.levels     = 1;
            range.baseLayer  = 0;
            range.layers     = 1;
            range.aspectMask = AspectFlagBit::COLOR_BIT;
        }
        return range;
    }

    PipelineStageFlags RenderGraph::StageForAccess(AccessFlags access, uint32_t passIndex) const
    {
        if (access == AccessFlagBit::NONE) {
            return PipelineStageBit::TOP;
        }
        if (access & AccessFlagBit::RTV) {
            return PipelineStageBit::COLOR_OUTPUT;
        }
        if (access & (AccessFlagBit::DSV | AccessFlagBit::DSV_READ)) {
            return (PipelineStageBit::EARLY_FRAGMENT | PipelineStageBit::LATE_FRAGMENT);
        }
        if (access & (AccessFlagBit::COPY_SRC | AccessFlagBit::COPY_DST)) {
            return PipelineStageBit::TRANSFER;
        }
        if (access & (AccessFlagBit::VERTEX_BUFFER | AccessFlagBit::INDEX_BUFFER | AccessFlagBit::INDIRECT_BUFFER)) {
            return PipelineStageBit::VERTEX_INPUT;
        }
        if (access & AccessFlagBit::PRESENT) {
            return PipelineStageBit::BOTTOM;
        }

        // SRV / UAV / CBV: stage depends on the pass type
        if (passIndex != INVALID_INDEX && passIndex < mPasses.size()) {
            const auto &pass = mPasses[passIndex];
            if (std::holds_alternative<ComputePassTag>(pass.tag)) {
                return PipelineStageBit::COMPUTE_SHADER;
            }
        }
        return (PipelineStageBit::VERTEX_SHADER | PipelineStageBit::FRAGMENT_SHADER);
    }

    BarrierInfo RenderGraph::MakeImageBarrier(Image *image, AccessFlags src, AccessFlags dst, const ImageSubRange &range,
                                              PipelineStageFlags srcStage, PipelineStageFlags dstStage) const
    {
        ImageBarrierInfo ib{};
        ib.image     = image;
        ib.subRange  = range;
        ib.srcAccess = src;
        ib.dstAccess = dst;
        ib.oldLayout = InferLayoutForAccess(src);
        ib.newLayout = InferLayoutForAccess(dst);

        BarrierInfo info{};
        info.srcStage = srcStage;
        info.dstStage = dstStage;
        info.imageBarriers.push_back(ib);
        return info;
    }

    BarrierInfo RenderGraph::MakeBufferBarrier(Buffer *buffer, AccessFlags src, AccessFlags dst,
                                               PipelineStageFlags srcStage, PipelineStageFlags dstStage) const
    {
        BufferBarrierInfo bb{};
        bb.buffer    = buffer;
        bb.srcAccess = src;
        bb.dstAccess = dst;

        BarrierInfo info{};
        info.srcStage = srcStage;
        info.dstStage = dstStage;
        info.bufferBarriers.push_back(bb);
        return info;
    }

    void RenderGraph::DeriveBarriers()
    {
        for (auto &pass : mPasses) {
            pass.frontBarriers.clear();
        }
        mFinalBarriers.clear();

        for (uint32_t resIndex = 0; resIndex < mResources.size(); ++resIndex) {
            const auto &node = mResources[resIndex];
            if (node.accesses.empty()) {
                continue;
            }

            const bool isImage  = std::holds_alternative<TransientImageTag>(node.tag) ||
                                  std::holds_alternative<ImportImageTag>(node.tag);
            const bool isBuffer = std::holds_alternative<TransientBufferTag>(node.tag) ||
                                  std::holds_alternative<ImportBufferTag>(node.tag);

            Image  *image  = isImage ? mResolvedImages[resIndex] : nullptr;
            Buffer *buffer = isBuffer ? mResolvedBuffers[resIndex] : nullptr;
            if ((isImage && image == nullptr) || (isBuffer && buffer == nullptr)) {
                continue;
            }

            const ImageSubRange subRange = isImage ? FullSubRange(resIndex) : ImageSubRange{};

            AccessFlags initialAccess = AccessFlagBit::NONE;
            if (std::holds_alternative<ImportImageTag>(node.tag)) {
                initialAccess = mImportImages[node.payloadIndex].importAccess;
            } else if (std::holds_alternative<ImportBufferTag>(node.tag)) {
                initialAccess = mImportBuffers[node.payloadIndex].importAccess;
            }

            const auto &first = node.accesses.front();
            if (first.access != initialAccess) {
                BarrierInfo info = isImage
                                       ? MakeImageBarrier(image, initialAccess, first.access, subRange,
                                                          StageForAccess(initialAccess, INVALID_INDEX), StageForAccess(first.access, first.pass))
                                       : MakeBufferBarrier(buffer, initialAccess, first.access,
                                                           StageForAccess(initialAccess, INVALID_INDEX), StageForAccess(first.access, first.pass));
                if (mPasses[first.pass].live) {
                    mPasses[first.pass].frontBarriers.push_back(std::move(info));
                }
            }

            for (size_t i = 0; i + 1 < node.accesses.size(); ++i) {
                const auto &prev = node.accesses[i];
                const auto &next = node.accesses[i + 1];
                if (prev.pass == next.pass || prev.access == next.access) {
                    continue;
                }
                if (!mPasses[next.pass].live) {
                    continue;
                }

                BarrierInfo info = isImage
                                       ? MakeImageBarrier(image, prev.access, next.access, subRange,
                                                          StageForAccess(prev.access, prev.pass), StageForAccess(next.access, next.pass))
                                       : MakeBufferBarrier(buffer, prev.access, next.access,
                                                           StageForAccess(prev.access, prev.pass), StageForAccess(next.access, next.pass));
                mPasses[next.pass].frontBarriers.push_back(std::move(info));
            }

            const auto &last = node.accesses.back();
            if (std::holds_alternative<ImportImageTag>(node.tag)) {
                const auto importAccess = mImportImages[node.payloadIndex].importAccess;
                if (importAccess != AccessFlagBit::NONE && last.access != importAccess && mPasses[last.pass].live) {
                    mFinalBarriers.push_back(MakeImageBarrier(image, last.access, importAccess, subRange,
                                                              StageForAccess(last.access, last.pass), StageForAccess(importAccess, INVALID_INDEX)));
                }
            }
        }
    }

} // namespace sky::aurora
