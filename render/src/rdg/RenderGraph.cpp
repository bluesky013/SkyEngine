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
        AddVertex("root", Root{}, *this);
    }

    void ResourceGraph::AddImage(const char *name, const GraphImage &image)
    {
        add_edge(0, AddVertex(name, image, *this), graph);
    }

    void ResourceGraph::ImportImage(const char *name, const rhi::ImagePtr &image)
    {
        add_edge(0, AddVertex(name, GraphImportImage{image}, *this), graph);
    }

    void ResourceGraph::AddImageView(const char *name, const char *source, const GraphImageView &view)
    {
        auto src = FindVertex(source, *this);
        SKY_ASSERT(src != INVALID_VERTEX);

        auto dst = AddVertex(name, view, *this);
        sources[dst] = sources[src];
        add_edge(src, dst, graph);
    }

    void ResourceGraph::AddBuffer(const char *name, const GraphBuffer &buffer)
    {
        add_edge(0, AddVertex(name, buffer, *this), graph);
    }

    void ResourceGraph::ImportBuffer(const char *name, const rhi::BufferPtr &buffer)
    {
        add_edge(0, AddVertex(name, GraphImportBuffer{buffer}, *this), graph);
    }

    void ResourceGraph::AddBufferView(const char *name, const char *source, const GraphBufferView &view)
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
        AddVertex("root", Root{}, *this);
    }

    RasterPassBuilder RenderGraph::AddRasterPass(const char *name, uint32_t width, uint32_t height)
    {
        auto vtx = AddVertex(name, RasterPass(width, height, &context->resources), *this);
        add_edge(0, vtx, graph);
        return RasterPassBuilder{*this, rasterPasses[polymorphicDatas[vtx]], vtx};
    }

    ComputePassBuilder RenderGraph::AddComputePass(const char *name)
    {
        auto vtx = AddVertex(name, ComputePass{&context->resources}, *this);
        add_edge(0, vtx, graph);
        return ComputePassBuilder{*this, vtx};
    }

    CopyPassBuilder RenderGraph::AddCopyPass(const char *name)
    {
        auto vtx = AddVertex(name, CopyBlitPass{&context->resources}, *this);
        add_edge(0, vtx, graph);
        return CopyPassBuilder{*this, vtx};
    }

    void RenderGraph::AddPresentPass(const char *name, const rhi::SwapChainPtr &swapchain)
    {
        auto vtx = AddVertex(name, PresentPass(swapchain, &context->resources), *this);
        add_edge(0, vtx, graph);
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
            accessGraph.resources[Index(resAccessID, accessGraph)].accesses.resize(sourceRange.range * sourceRange.layers, rhi::AccessFlagBit::NONE);
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
        auto res = FindVertex(attachment.name.c_str(), rdg.resourceGraph);
        SKY_ASSERT(res != INVALID_VERTEX);

        pass.attachmentVertex.emplace_back(res);
        pass.attachments.emplace_back(attachment);
        pass.clearValues.emplace_back(clear);
        return *this;
    }

    RasterSubPassBuilder RasterPassBuilder::AddRasterSubPass(const std::string &name)
    {
        auto dst = AddVertex(name.c_str(), RasterSubPass{&rdg.context->resources}, rdg);
        add_edge(vertex, dst, rdg.graph);
        auto &rasterPass = rdg.rasterPasses[rdg.polymorphicDatas[vertex]];
        auto &subPass = rdg.subPasses[rdg.polymorphicDatas[dst]];
        subPass.parent = vertex;
        subPass.subPassID = static_cast<uint32_t>(rasterPass.subPasses.size());
        rasterPass.subPasses.emplace_back(dst);
        return RasterSubPassBuilder{rdg, rasterPass, subPass, dst};
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddColor(const std::string &name, const ResourceAccess& access)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.colors.emplace_back(RasterAttachmentRef{name, access, attachmentIndex});
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::COLOR, access});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddResolve(const std::string &name, const ResourceAccess& access)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.resolves.emplace_back(RasterAttachmentRef{name, access, attachmentIndex});
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::RESOLVE, access});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddInput(const std::string &name, const ResourceAccess& access)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.inputs.emplace_back(RasterAttachmentRef{name, access, attachmentIndex});
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::INPUT, access});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddColorInOut(const std::string &name)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.inputs.emplace_back(RasterAttachmentRef{name, ResourceAccessBit::READ_WRITE, attachmentIndex});
        subPass.colors.emplace_back(RasterAttachmentRef{name, ResourceAccessBit::READ_WRITE, attachmentIndex});
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::INPUT | RasterTypeBit::COLOR, ResourceAccessBit::READ_WRITE});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddDepthStencil(const std::string &name, const ResourceAccess& access)
    {
        uint32_t attachmentIndex = GetAttachmentIndex(name);
        subPass.depthStencil = RasterAttachmentRef{name, access, attachmentIndex};
        return AddRasterView(name, pass.attachmentVertex[attachmentIndex], RasterView{RasterTypeBit::DEPTH_STENCIL, access});
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddRasterView(const std::string &name, VertexType resVertex, const RasterView &view)
    {
        SKY_ASSERT(subPass.rasterViews.emplace(name, view).second);
        rdg.AddDependency(resVertex, vertex, DependencyInfo{view.type, view.access, {}});
        return *this;
    }

    uint32_t RasterSubPassBuilder::GetAttachmentIndex(const std::string &name)
    {
        auto iter = std::find_if(pass.attachments.begin(), pass.attachments.end(), [&name](const RasterAttachment &attachment){
            return name == attachment.name;
        });
        SKY_ASSERT(iter != pass.attachments.end());
        return static_cast<uint32_t>(std::distance(pass.attachments.begin(), iter));
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddComputeView(const std::string &name, const ComputeView &view)
    {
        auto res = FindVertex(name.c_str(), rdg.resourceGraph);
        SKY_ASSERT(res != INVALID_VERTEX);

        subPass.computeViews.emplace(name, view);
        rdg.AddDependency(res, vertex, DependencyInfo{view.type, view.access, view.visibility});
        return *this;
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddSceneView(const std::string &name, const SceneView *sceneView)
    {
        auto rsv = RasterSceneView(&rdg.context->resources);
        rsv.sceneView = sceneView;

        auto dst = AddVertex(name.c_str(), rsv, rdg);
        add_edge(vertex, dst, rdg.graph);
        return *this;
    }
}
