//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraph.h>
#include <render/rdg/RenderGraphCompiler.h>

namespace sky::rdg {

    void RenderGraph::AddImage(const char *name, const GraphImage &image)
    {
        AddVertex(name, image, *this);
    }

    void RenderGraph::ImportImage(const char *name, const rhi::ImagePtr &image)
    {
        AddVertex(name, GraphImportImage{image}, *this);
    }

    void RenderGraph::AddImageView(const char *name, const char *source, const GraphImageView &view)
    {
        auto src = FindVertex(source);
        SKY_ASSERT(src != INVALID_VERTEX);
        auto dst = AddVertex(name, view, *this);
        AddEdge(src, dst, resourceGraph);
    }

    void RenderGraph::AddBuffer(const char *name, const GraphBuffer &buffer)
    {
        AddVertex(name, buffer, *this);
    }

    void RenderGraph::ImportBuffer(const char *name, const rhi::BufferPtr &buffer)
    {
        AddVertex(name, GraphImportBuffer{buffer}, *this);
    }

    void RenderGraph::AddBufferView(const char *name, const char *source, const GraphBufferView &view)
    {
        auto src = FindVertex(source);
        SKY_ASSERT(src != INVALID_VERTEX);
        auto dst = AddVertex(name, view, *this);
        AddEdge(src, dst, resourceGraph);
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

    void RenderGraph::Compile()
    {
        RenderResourceCompiler compiler;
//        boost::depth_first_search(resourceGraph, boost::visitor(compiler));
    }
}