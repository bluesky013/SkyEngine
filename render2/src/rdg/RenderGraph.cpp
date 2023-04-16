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

    RasterPassBuilder RenderGraph::AddRasterPass(const char *name)
    {
        return RasterPassBuilder{*this};
    }

    ComputePassBuilder RenderGraph::AddComputePass(const char *name)
    {
        return ComputePassBuilder{*this};
    }

    CopyPassBuilder RenderGraph::AddCopyPass(const char *name)
    {
        return CopyPassBuilder{*this};
    }

    VertexType RenderGraph::FindVertex(const char *name)
    {
        auto iter = std::find(names.begin(), names.end(), name);
        return iter == names.end() ? INVALID_VERTEX : static_cast<VertexType>(std::distance(names.begin(), iter));
    }

}