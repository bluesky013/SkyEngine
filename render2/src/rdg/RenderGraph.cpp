//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraph.h>
#include <render/rdg/RenderGraphCompiler.h>

namespace sky::rdg {

    RenderGraph::RenderGraph(RenderGraphContext *ctx)
        : context(ctx)
        , vertices(&ctx->resources)
        , names(&ctx->resources)
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

    void RenderGraph::AddImage(const char *name, const GraphImage &image)
    {
        add_edge(0, AddVertex(name, image, *this), resourceGraph);
    }

    void RenderGraph::ImportImage(const char *name, const rhi::ImagePtr &image)
    {
        add_edge(0, AddVertex(name, GraphImportImage{image}, *this), resourceGraph);
    }

    void RenderGraph::AddImageView(const char *name, const char *source, const GraphImageView &view)
    {
        auto src = FindVertex(source);
        SKY_ASSERT(src != INVALID_VERTEX);
        auto dst = AddVertex(name, view, *this);
        add_edge(src, dst, resourceGraph);
    }

    void RenderGraph::AddBuffer(const char *name, const GraphBuffer &buffer)
    {
        add_edge(0, AddVertex(name, buffer, *this), resourceGraph);
    }

    void RenderGraph::ImportBuffer(const char *name, const rhi::BufferPtr &buffer)
    {
        add_edge(0, AddVertex(name, GraphImportBuffer{buffer}, *this), resourceGraph);
    }

    void RenderGraph::AddBufferView(const char *name, const char *source, const GraphBufferView &view)
    {
        auto src = FindVertex(source);
        SKY_ASSERT(src != INVALID_VERTEX);
        auto dst = AddVertex(name, view, *this);
        add_edge(src, dst, resourceGraph);
    }

    RasterPassBuilder RenderGraph::AddRasterPass(const char *name, uint32_t width, uint32_t height)
    {
        auto vtx = AddVertex(name, RasterPass(width, height, &context->resources), *this);
        add_edge(0, vtx, passGraph);
        return RasterPassBuilder{*this, rasterPasses[polymorphicDatas[vtx]], vtx};
    }

    RasterSubPassBuilder RenderGraph::AddRasterSubPass(const char *name, const char *pass)
    {
        auto src = FindVertex(pass);
        SKY_ASSERT(src != INVALID_VERTEX);
        auto dst = AddVertex(name, RasterSubPass{&context->resources}, *this);
        add_edge(src, dst, passGraph);
        return RasterSubPassBuilder{*this, rasterPasses[polymorphicDatas[src]], subPasses[polymorphicDatas[dst]], dst};
    }

    ComputePassBuilder RenderGraph::AddComputePass(const char *name)
    {
        auto vtx = AddVertex(name, ComputePass{&context->resources}, *this);
        add_edge(0, vtx, passGraph);
        return ComputePassBuilder{*this, vtx};
    }

    CopyPassBuilder RenderGraph::AddCopyPass(const char *name)
    {
        auto vtx = AddVertex(name, CopyBlitPass{&context->resources}, *this);
        add_edge(0, vtx, passGraph);
        return CopyPassBuilder{*this, vtx};
    }

    VertexType RenderGraph::FindVertex(const char *name)
    {
        auto iter = std::find(names.begin(), names.end(), name);
        return iter == names.end() ? INVALID_VERTEX : static_cast<VertexType>(std::distance(names.begin(), iter));
    }


    RasterSubPassBuilder &RasterSubPassBuilder::AddRasterView(const char *name, const RasterView &view)
    {
        auto res = graph.FindVertex(name);
        SKY_ASSERT(res != INVALID_VERTEX);

        subPass.rasterViews.emplace(name, view);
        if (view.access & ResourceAccessBit::READ) {
            add_edge(res, vertex, graph.dependencyGraph);
        }
        if (view.access & ResourceAccessBit::WRITE) {
            add_edge(vertex, res, graph.dependencyGraph);
        }
        return *this;
    }

    RasterSubPassBuilder &RasterSubPassBuilder::AddComputeView(const char *name, const ComputeView &view)
    {
        auto res = graph.FindVertex(name);
        SKY_ASSERT(res != INVALID_VERTEX);

        subPass.computeViews.emplace(name, view);
        if (view.access & ResourceAccessBit::READ) {
            add_edge(res, vertex, graph.dependencyGraph);
        }
        if (view.access & ResourceAccessBit::WRITE) {
            add_edge(vertex, res, graph.dependencyGraph);
        }
        return *this;
    }
}
