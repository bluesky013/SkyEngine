//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraph.h>
#include <render/rdg/RenderGraphCompiler.h>

namespace sky::rdg {

    void RenderGraph::AddTexture(const std::string &name, const TextureAttachmentDesc &texture)
    {
    }

    void RenderGraph::ImportTexture(const std::string &name, const rhi::ImagePtr &image)
    {
    }

    void RenderGraph::AddBuffer(const std::string &name, const BufferAttachmentDesc &buffer)
    {
    }

    void RenderGraph::ImportBuffer(const std::string &name, const rhi::BufferPtr &buffer)
    {
    }

    RasterPassBuilder RenderGraph::AddRasterPass(const std::string &name)
    {
        return RasterPassBuilder{};
    }

    ComputePassBuilder RenderGraph::AddComputePass(const std::string &name)
    {
        return ComputePassBuilder{};
    }

    CopyPassBuilder RenderGraph::AddCopyPass(const std::string &name)
    {
        return CopyPassBuilder{};
    }


    void RenderGraph::Compile()
    {
        RenderGraphCompiler compiler;

        auto v1 = boost::add_vertex(nodeGraph);
        auto v2 = boost::add_vertex(nodeGraph);

        boost::add_edge(v1, v2, nodeGraph);

        boost::depth_first_search(nodeGraph, boost::visitor(compiler));
    }

}