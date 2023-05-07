//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraph.h>
#include <render/rdg/RenderGraphVisitors.h>
#include <sstream>

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

    RasterSubPassBuilder RenderGraph::AddRasterSubPass(const char *name, const char *pass)
    {
        auto src = FindVertex(pass, *this);
        SKY_ASSERT(src != INVALID_VERTEX);
        auto dst = AddVertex(name, RasterSubPass{&context->resources}, *this);
        add_edge(src, dst, graph);
        return RasterSubPassBuilder{*this, rasterPasses[polymorphicDatas[src]], subPasses[polymorphicDatas[dst]], dst};
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

    static std::string GetRefNodeName(const char *resName, const char *passName, uint32_t refId)
    {
        std::stringstream ss;
        ss << passName << '/' << resName << '[' << refId << ']';
        return ss.str();
    }

    void RenderGraph::AddDependency(const char *name, VertexType passId, const AccessEdge &edge)
    {
        auto res = FindVertex(name, resourceGraph);
        SKY_ASSERT(res != INVALID_VERTEX);

        auto read = edge.access & ResourceAccessBit::READ;
        auto write = edge.access & ResourceAccessBit::WRITE;

        auto &lastAccess = resourceGraph.lastAccesses[res];
        auto lastResAccessID = lastAccess;
        auto passAccessID = accessNodes[passId];

        if (read && lastResAccessID == INVALID_VERTEX) {
            lastAccess = AddVertex(AccessRes{res}, accessGraph);
        }

        if (read || lastResAccessID != INVALID_VERTEX) {
            auto [ed, sec] = add_edge(lastAccess, passAccessID, accessGraph.graph);
            accessGraph.graph[ed] = edge;
        }

        if (write) {
            lastAccess = AddVertex(AccessRes{res}, accessGraph);
            auto [ed, sec] = add_edge(passAccessID, lastAccess, accessGraph.graph);
            accessGraph.graph[ed] = edge;
        }
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddRasterView(const char *name, const RasterView &view)
    {
        subPass.rasterViews.emplace(name, view);
        graph.AddDependency(name, vertex, AccessEdge{view.type, view.access, {}});
        return *this;
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddComputeView(const char *name, const ComputeView &view)
    {
        subPass.computeViews.emplace(name, view);
        graph.AddDependency(name, vertex, AccessEdge{view.type, view.access, view.visibility});
        return *this;
    }
}
