//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include<boost/graph/adjacency_list.hpp>
#include<boost/graph/properties.hpp>
#include<boost/property_map/property_map.hpp>

#include <render/rdg/RenderGraphTypes.h>
#include <rhi/Image.h>
#include <rhi/Buffer.h>

namespace sky::rdg {

    struct RasterView {
        std::string attachmentName;
        ResourceAccess access = ResourceAccess::READ;
        rhi::ImageSubRange range;
        rhi::ClearValue clear;
        rhi::LoadOp  loadOp  = rhi::LoadOp::DONT_CARE;
        rhi::StoreOp storeOp = rhi::StoreOp::DONT_CARE;
        rhi::LoadOp  stencilLoad = rhi::LoadOp::DONT_CARE;
        rhi::StoreOp stencilStore = rhi::StoreOp::DONT_CARE;
    };

    struct ComputeView {
        std::string attachmentName;
        ResourceAccess access = ResourceAccess::READ;
        bool isUAV = false;
    };

    struct CopyView {
        std::string srcName;
        rhi::ImageSubRange srcRange;
        rhi::Extent3D srcExtent;
        rhi::Offset3D srcOffset;

        std::string dstName;
        rhi::ImageSubRange dstRange;
        rhi::Extent3D dstExtent;
        rhi::Offset3D dstOffset;
    };

    struct RasterPassBuilder {
        void AddRasterView(const RasterView &view);
        void AddComputeView(const ComputeView &view);
    };

    struct ComputePassBuilder {
        void AddComputeView(const ComputeView &view);
    };

    struct CopyPassBuilder {
        void AddCopyView(const CopyView &view);
    };

    class RenderGraph {
    public:
        RenderGraph()  = default;
        ~RenderGraph() = default;

        // pass node dag
        using NodeGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS>;

        // dependency graph
        using DependencyGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;

        // resource graph
        using ResourceGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS>;

        void AddTexture(const std::string &name, const TextureAttachmentDesc &texture);
        void ImportTexture(const std::string &name, const rhi::ImagePtr &image);

        void AddBuffer(const std::string &name, const BufferAttachmentDesc &attachment);
        void ImportBuffer(const std::string &name, const rhi::BufferPtr &buffer);

        RasterPassBuilder AddRasterPass(const std::string &name);
        ComputePassBuilder AddComputePass(const std::string &name);
        CopyPassBuilder AddCopyPass(const std::string &name);

        void Compile();

    private:
        NodeGraph nodeGraph;
        DependencyGraph depGraph;
        ResourceGraph resourceGraph;
    };

} // namespace sky