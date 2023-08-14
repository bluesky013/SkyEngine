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

    AccessGraph::AccessGraph(RenderGraphContext *ctx)
        : context(ctx)
        , vertices(&ctx->resources)
        , tags(&ctx->resources)
        , polymorphicDatas(&ctx->resources)
    {
    }

    AccessRange GetAccessRange(const RenderGraph &graph, VertexType resID)
    {
        const auto &resourceGraph = graph.resourceGraph;
        AccessRange range = {};

        std::visit(Overloaded{
            [&](const ImageTag &tag) {
                const auto &image = resourceGraph.images[Index(resID, resourceGraph)];
                range = {
                    0, image.desc.mipLevels,
                    0, image.desc.arrayLayers,
                    rhi::GetAspectFlagsByFormat(image.desc.format)
                };
            },
            [&](const ImportImageTag &tag) {
                const auto &image = resourceGraph.importImages[Index(resID, resourceGraph)];
                const auto &desc = image.desc.image->GetDescriptor();
                range = {
                    0, desc.mipLevels,
                    0, desc.arrayLayers,
                    rhi::GetAspectFlagsByFormat(desc.format)
                };
            },
            [&](const ImageViewTag &tag) {
                const auto &view = resourceGraph.imageViews[Index(resID, resourceGraph)];
                range = {
                    view.desc.view.subRange.baseLevel, view.desc.view.subRange.levels,
                    view.desc.view.subRange.baseLayer, view.desc.view.subRange.layers,
                    view.desc.view.subRange.aspectMask
                };
            },
            [&](const BufferTag &tag) {
                const auto &buffer = resourceGraph.buffers[Index(resID, resourceGraph)];
                range.range = buffer.desc.size;
            },
            [&](const ImportBufferTag &tag) {
                const auto &buffer = resourceGraph.importBuffers[Index(resID, resourceGraph)];
                const auto &desc = buffer.desc.buffer->GetBufferDesc();
                range.range = desc.size;
            },
            [&](const BufferViewTag &tag) {
                const auto &view = resourceGraph.bufferViews[Index(resID, resourceGraph)];
                range.base = view.desc.view.offset;
                range.range = view.desc.view.range;
            },
            [&](const auto &) {}
        }, rdg::Tag(resID, resourceGraph));
        return range;
    }

    void RenderGraph::AddDependency(VertexType resID, VertexType passId, const DependencyInfo &deps)
    {
        VertexType sourceID = Source(resID, resourceGraph);
        auto passAccessID = accessNodes[passId];
        auto &resAccessID = resourceGraph.lastAccesses[sourceID];
        auto subRange = GetAccessRange(*this, resID);

        if (resAccessID == INVALID_VERTEX) {
            subRange = GetAccessRange(*this, sourceID);
            resAccessID = AddVertex(AccessRes{sourceID, subRange}, accessGraph);
        }
        auto &lastAccessRes = accessGraph.resources[Index(resAccessID, accessGraph)];

        auto read = deps.access & ResourceAccessBit::READ;
        auto write = deps.access & ResourceAccessBit::WRITE;
        auto layout = GetImageLayout(deps);

        auto intersection = Intersection(lastAccessRes.subRange, subRange);
        auto versionChanged = (lastAccessRes.layout != layout) || (write && intersection);

        if (versionChanged) {
            {
                auto [ed, sec] = add_edge(resAccessID, passAccessID, accessGraph.graph);
                accessGraph.graph[ed] = {deps, subRange};
            }
            resAccessID = AddVertex(AccessRes{sourceID, subRange, layout}, accessGraph);
            {
                auto [ed, sec] = add_edge(passAccessID, resAccessID, accessGraph.graph);
                accessGraph.graph[ed] = {deps, subRange};
            }
        } else {
            if (write) {
                auto [ed, sec] = add_edge(passAccessID, resAccessID, accessGraph.graph);
                accessGraph.graph[ed] = {deps, subRange};
            } else {
                auto [ed, sec] = add_edge(resAccessID, passAccessID, accessGraph.graph);
                accessGraph.graph[ed] = {deps, subRange};
            }
            MergeSubRange(lastAccessRes.subRange, subRange);
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

    RasterSubPassBuilder &RasterSubPassBuilder::AddSceneView(const std::string &name, const ViewPtr &sceneView)
    {
        auto rsv = RasterSceneView(&rdg.context->resources);
        rsv.sceneView = sceneView;

        auto dst = AddVertex(name.c_str(), rsv, rdg);
        add_edge(vertex, dst, rdg.graph);
        return *this;
    }
}
