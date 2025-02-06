//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraph.h>
#include <rhi/Decode.h>
#include <sstream>
#include <render/rdg/AccessUtils.h>

namespace sky::rdg {

    ResourceGraph::ResourceGraph(RenderGraphContext *ctx)
        : context(ctx)
        , vertices(&ctx->resources)
        , names(&ctx->resources)
        , sources(&ctx->resources)
        , lastAccesses(&ctx->resources)
        , tags(&ctx->resources)
        , polymorphicDatas(&ctx->resources)
        , images(&ctx->resources)
        , importImages(&ctx->resources)
        , imageViews(&ctx->resources)
        , buffers(&ctx->resources)
        , importBuffers(&ctx->resources)
        , bufferViews(&ctx->resources)
    {
        AddVertex(Name("root"), Root{}, *this);
    }

    void ResourceGraph::AddImage(const Name &name, const GraphImage &image)
    {
        add_edge(0, AddVertex(name, image, *this), graph);
    }


    void ResourceGraph::ImportImage(const Name &name, const rhi::ImagePtr &image)
    {
        ImportImage(name, image, rhi::ImageViewType::VIEW_2D, rhi::AccessFlagBit::NONE);
    }

    void ResourceGraph::ImportImage(const Name &name, const rhi::ImagePtr &image, rhi::ImageViewType viewType)
    {
        ImportImage(name, image, viewType, rhi::AccessFlagBit::NONE);
    }

    void ResourceGraph::ImportImage(const Name &name, const rhi::ImagePtr &image, rhi::ImageViewType viewType, const rhi::AccessFlags &currentAccess)
    {
        add_edge(0, AddVertex(name, GraphImportImage{image, viewType, currentAccess}, *this), graph);
    }

    void ResourceGraph::ImportSwapChain(const Name &name, const rhi::SwapChainPtr &swapchain)
    {
        add_edge(0, AddVertex(name, GraphSwapChain{swapchain}, *this), graph);
    }
#ifdef SKY_ENABLE_XR
    void ResourceGraph::ImportXRSwapChain(const Name &name, const rhi::XRSwapChainPtr &swapchain)
    {
        add_edge(0, AddVertex(name, GraphXRSwapChain{swapchain}, *this), graph);
    }
#endif
    void ResourceGraph::AddImageView(const Name &name, const Name &source, const GraphImageView &view)
    {
        auto src = FindVertex(source, *this);
        SKY_ASSERT(src != INVALID_VERTEX);

        auto dst = AddVertex(name, view, *this);
        sources[dst] = sources[src];
        add_edge(src, dst, graph);
    }

    void ResourceGraph::AddBuffer(const Name &name, const GraphBuffer &buffer)
    {
        add_edge(0, AddVertex(name, buffer, *this), graph);
    }

    void ResourceGraph::ImportBuffer(const Name &name, const rhi::BufferPtr &buffer)
    {
        ImportBuffer(name, buffer, rhi::AccessFlagBit::NONE);
    }

    void ResourceGraph::ImportBuffer(const Name &name, const rhi::BufferPtr &buffer, const rhi::AccessFlags &flags)
    {
        add_edge(0, AddVertex(name, GraphImportBuffer{buffer, flags}, *this), graph);
    }

    void ResourceGraph::ImportUBO(const Name &name, const RDUniformBufferPtr &ubo)
    {
        AddVertex(name, GraphConstantBuffer{ubo}, *this);
    }

    void ResourceGraph::AddBufferView(const Name &name, const Name &source, const GraphBufferView &view)
    {
        auto src = FindVertex(source, *this);
        SKY_ASSERT(src != INVALID_VERTEX);

        auto dst = AddVertex(name, view, *this);
        sources[dst] = sources[src];
        add_edge(src, dst, graph);
    }

    RenderGraph::RenderGraph(RenderGraphContext *ctx)
        : context(ctx)
        , vertices(&ctx->resources)
        , names(&ctx->resources)
        , accessNodes(&ctx->resources)
        , tags(&ctx->resources)
        , polymorphicDatas(&ctx->resources)
        , rasterPasses(&ctx->resources)
        , subPasses(&ctx->resources)
        , computePasses(&ctx->resources)
        , copyBlitPasses(&ctx->resources)
        , presentPasses(&ctx->resources)
        , resourceGraph(ctx)
        , accessGraph(ctx)
    {
        AddVertex(Name("root"), Root{}, *this);
    }

    RasterPassBuilder RenderGraph::AddRasterPass(const Name &name, uint32_t width, uint32_t height)
    {
        auto vtx = AddVertex(name, RasterPass(width, height, &context->resources), *this);
        add_edge(0, vtx, graph);
        return RasterPassBuilder{*this, rasterPasses[polymorphicDatas[vtx]], vtx};
    }

    ComputePassBuilder RenderGraph::AddComputePass(const Name &name)
    {
        auto vtx = AddVertex(name, ComputePass{&context->resources}, *this);
        add_edge(0, vtx, graph);
        return ComputePassBuilder{*this, computePasses[polymorphicDatas[vtx]], vtx};
    }

    CopyPassBuilder RenderGraph::AddCopyPass(const Name &name)
    {
        auto vtx = AddVertex(name, CopyBlitPass{&context->resources}, *this);
        add_edge(0, vtx, graph);
        return CopyPassBuilder{*this, copyBlitPasses[polymorphicDatas[vtx]], vtx};
    }

    void RenderGraph::AddUploadPass(const Name &name, const UploadPass &upload)
    {
        auto vtx = AddVertex(name, upload, *this);
        add_edge(0, vtx, graph);
    }

    void RenderGraph::AddPresentPass(const Name &name, const Name &resName)
    {
        auto resID = FindVertex(resName, resourceGraph);
        SKY_ASSERT(resID != INVALID_VERTEX);

        std::visit(Overloaded{
            [&](const ImportSwapChainTag &tag) {
                const auto &res = resourceGraph.swapChains[Index(resID, resourceGraph)];
                auto vtx = AddVertex(name, PresentPass(resID, res.desc.swapchain, &context->resources), *this);
                add_edge(0, vtx, graph);
                AddDependency(resID, vtx, DependencyInfo{PresentType::PRESENT, ResourceAccessBit::READ, {}});
            },
#ifdef SKY_ENABLE_XR
            [&](const ImportXRSwapChainTag &tag) {
                const auto &res = resourceGraph.xrSwapChains[Index(resID, resourceGraph)];
                auto vtx = AddVertex(name, PresentPass(resID, res.desc.swapchain, &context->resources), *this);
                add_edge(0, vtx, graph);
                AddDependency(resID, vtx, DependencyInfo{PresentType::PRESENT, ResourceAccessBit::READ, {}});
            },
#endif
            [&](const auto &) {}
        }, rdg::Tag(resID, resourceGraph));
    }

    AccessGraph::AccessGraph(RenderGraphContext *ctx)
        : context(ctx)
        , vertices(&ctx->resources)
        , tags(&ctx->resources)
        , polymorphicDatas(&ctx->resources)
    {
    }

    bool RenderGraph::CheckVersionChanged(const AccessRes &lastAccess, const DependencyInfo &deps, const AccessRange &subRange)
    {
        auto write = deps.access & ResourceAccessBit::WRITE;
        const auto accessFlag = GetAccessFlags(deps);
        for (auto i = subRange.base; i < subRange.range; ++i) {
            for (auto j = subRange.layer; j < subRange.layers; ++j) {
                if (lastAccess.accesses[i * subRange.layers + j] != accessFlag) {
                    return true;
                }
            }
        }

        return static_cast<bool>(write);
    }

    void RenderGraph::FillAccessFlag(AccessRes &res, const AccessRange &subRange, const rhi::AccessFlags& accessFlag) const
    {
        const auto sourceRange = GetAccessRange(*this, res.resID);
        for (auto i = 0U; i < subRange.range; ++i) {
            const auto mip = subRange.base + i;
            for (auto j = 0U; j < subRange.layers; ++j) {
                const auto layer = subRange.layer + j;
                res.accesses[mip * sourceRange.layers + layer] = accessFlag;
            }
        }
    }

    AccessRes RenderGraph::GetMergedAccessRes(const AccessRes &lastAccess,
                                              const rhi::AccessFlags& accessFlag,
                                              const AccessRange &subRange,
                                              VertexType passAccessID,
                                              VertexType nextAccessResID) const
    {
        AccessRes res = {lastAccess.resID, passAccessID, nextAccessResID, subRange, lastAccess.accesses};
        FillAccessFlag(res, subRange, accessFlag);
        return res;
    }

    void RenderGraph::AddDependency(VertexType resID, VertexType passId, const DependencyInfo &deps)
    {
        VertexType sourceID = Source(resID, resourceGraph);
        auto passAccessID = accessNodes[passId];
        auto &resAccessID = resourceGraph.lastAccesses[sourceID];
        auto subRange = GetAccessRange(*this, resID);

        if (resAccessID == INVALID_VERTEX) {
            const auto sourceRange = GetAccessRange(*this, sourceID);
            resAccessID = AddVertex(AccessRes{sourceID, 0, INVALID_VERTEX, sourceRange}, accessGraph);
            add_edge(0, resAccessID, accessGraph.graph);
            auto importFlags = GetImportAccessFlags(*this, sourceID);
            accessGraph.resources[Index(resAccessID, accessGraph)].accesses.resize(sourceRange.range * sourceRange.layers, importFlags);
        }
        auto &lastAccessRes = accessGraph.resources[Index(resAccessID, accessGraph)];

        auto write = deps.access & ResourceAccessBit::WRITE;
        bool crossPass = lastAccessRes.inAccessPassID != passAccessID;
        auto versionChanged = CheckVersionChanged(lastAccessRes, deps, subRange) && crossPass;

        const auto accessFlag = GetAccessFlags(deps);
        if (versionChanged) {
            {
                add_edge(resAccessID, passAccessID, accessGraph.graph);
            }

            const auto lastAccessID = resAccessID;
            resAccessID = AddVertex(GetMergedAccessRes(lastAccessRes, accessFlag, subRange, passAccessID, INVALID_VERTEX), accessGraph);
            accessGraph.resources[Index(lastAccessID, accessGraph)].nextAccessResID = resAccessID;
            {
                add_edge(passAccessID, resAccessID, accessGraph.graph);
            }
        } else {
            MergeSubRange(lastAccessRes.subRange, subRange);
            FillAccessFlag(lastAccessRes, subRange, accessFlag);
            if (!write) {
                add_edge(resAccessID, passAccessID, accessGraph.graph);
            }
        }
    }

    RasterPassBuilder &RasterPassBuilder::AddAttachment(const RasterAttachment &attachment, const rhi::ClearValue &clear)
    {
        auto res = FindVertex(attachment.name, rdg.resourceGraph);
        SKY_ASSERT(res != INVALID_VERTEX);

        pass.attachmentVertex.emplace_back(res);
        pass.attachments.emplace_back(attachment);
        pass.clearValues.emplace_back(clear);
        return *this;
    }

    RasterPassBuilder &RasterPassBuilder::AddCoRelationMasks(uint32_t mask)
    {
        pass.correlationMasks.emplace_back(mask);
        return *this;
    }

    RasterSubPassBuilder RasterPassBuilder::AddRasterSubPass(const Name &name)
    {
        auto dst = AddVertex(name, RasterSubPass{&rdg.context->resources}, rdg);
        add_edge(vertex, dst, rdg.graph);
        auto &rasterPass = rdg.rasterPasses[rdg.polymorphicDatas[vertex]];
        auto &subPass = rdg.subPasses[rdg.polymorphicDatas[dst]];
        subPass.parent = vertex;
        subPass.subPassID = static_cast<uint32_t>(rasterPass.subPasses.size());
        rasterPass.subPasses.emplace_back(dst);
        return RasterSubPassBuilder{rdg, rasterPass, subPass, dst};
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddColor(const Name &name, const ResourceAccess& access)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.colors.emplace_back(RasterAttachmentRef{name, access, attachmentIndex});
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::COLOR, access});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddResolve(const Name &name, const ResourceAccess& access)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.resolves.emplace_back(RasterAttachmentRef{name, access, attachmentIndex});
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::RESOLVE, access});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddInput(const Name &name, const ResourceAccess& access)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.inputs.emplace_back(RasterAttachmentRef{name, access, attachmentIndex});
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::INPUT, access});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddColorInOut(const Name &name)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.inputs.emplace_back(RasterAttachmentRef{name, ResourceAccessBit::READ_WRITE, attachmentIndex});
        subPass.colors.emplace_back(RasterAttachmentRef{name, ResourceAccessBit::READ_WRITE, attachmentIndex});
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::INPUT | RasterTypeBit::COLOR, ResourceAccessBit::READ_WRITE});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddDepthStencil(const Name &name, const ResourceAccess& access)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.depthStencil = RasterAttachmentRef{name, access, attachmentIndex};
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::DEPTH_STENCIL, access});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddRasterView(const Name &name, VertexType resVertex, const RasterView &view)
    {
        auto iter = subPass.rasterViews.emplace(name, view);
        SKY_ASSERT(iter.second);
        rdg.AddDependency(resVertex, vertex, DependencyInfo{view.type, view.access, {}});
        return *this;
    }

    uint32_t RasterSubPassBuilder::GetAttachmentIndex(const Name &name)
    {
        auto iter = std::find_if(pass.attachments.begin(), pass.attachments.end(), [&name](const RasterAttachment &attachment){
            return name == attachment.name;
        });
        SKY_ASSERT(iter != pass.attachments.end());
        return static_cast<uint32_t>(std::distance(pass.attachments.begin(), iter));
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddComputeView(const Name &name, const ComputeView &view)
    {
        auto res = FindVertex(name, rdg.resourceGraph);
        SKY_ASSERT(res != INVALID_VERTEX);

        subPass.computeViews.emplace(name, view);
        rdg.AddDependency(res, vertex, DependencyInfo{view.type, view.access, view.visibility});
        return *this;
    }

    RasterSubPassBuilder &RasterSubPassBuilder::SetViewMask(uint32_t mask)
    {
        subPass.viewMask = mask;
        return *this;
    }

    RasterQueueBuilder RasterSubPassBuilder::AddQueue(const Name &name)
    {
        auto res = AddVertex(name, RasterQueue(&rdg.context->resources, vertex), rdg);
        auto &queue = rdg.rasterQueues[rdg.polymorphicDatas[res]];
        add_edge(vertex, res, rdg.graph);
        return RasterQueueBuilder{rdg, queue, res};
    }

    FullScreenBuilder RasterSubPassBuilder::AddFullScreen(const Name &name)
    {
        auto res = AddVertex(name, FullScreenBlit(&rdg.context->resources, vertex), rdg);
        auto &fullscreen = rdg.fullScreens[rdg.polymorphicDatas[res]];
        add_edge(vertex, res, rdg.graph);
        return FullScreenBuilder{rdg, fullscreen, res};
    }

    CopyPassBuilder &CopyPassBuilder::AddCopyView(const CopyView &view)
    {
        blit.src = FindVertex(view.srcName, rdg.resourceGraph);
        blit.dst = FindVertex(view.dstName, rdg.resourceGraph);
        SKY_ASSERT(blit.src != INVALID_VERTEX);
        SKY_ASSERT(blit.dst != INVALID_VERTEX);

        blit.srcExt = view.srcExtent;
        blit.dstExt = view.dstExtent;

        blit.srcRange = view.srcRange;
        blit.dstRange = view.dstRange;

        rdg.AddDependency(blit.src, vertex, DependencyInfo{TransferType::SRC, ResourceAccessBit::READ, {}});
        rdg.AddDependency(blit.dst, vertex, DependencyInfo{TransferType::DST, ResourceAccessBit::WRITE, {}});
        return *this;
    }

    RasterQueueBuilder &RasterQueueBuilder::SetLayout(const RDResourceLayoutPtr &layout)
    {
        queue.layout = layout;
        return *this;
    }

    RasterQueueBuilder &RasterQueueBuilder::SetView(SceneView *view)
    {
        queue.sceneView = view;
        return *this;
    }

    RasterQueueBuilder &RasterQueueBuilder::SetRasterID(const Name &id)
    {
        queue.rasterID = id;
        return *this;
    }

    FullScreenBuilder &FullScreenBuilder::SetTechnique(const RDGfxTechPtr &tech)
    {
        fullscreen.technique = tech;
        fullscreen.program   = tech->RequestProgram({});
        return *this;
    }
} // namespace sky::rdg
