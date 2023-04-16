//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <limits>
#include <boost/graph/adjacency_list.hpp>
#include <core/std/Container.h>
#include <rhi/Core.h>
#include <rhi/Image.h>
#include <rhi/Buffer.h>
#include <rhi/Swapchain.h>

namespace sky::rdg {

    enum class ResourceResidency {
        TRANSIENT,
        PERSISTENT,
    };

    enum class ResourceAccess {
        READ,
        WRITE,
        READ_WRITE
    };

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

    struct RasterPassTag {};
    struct RasterSubPassTag {};
    struct ComputePassTag {};
    struct CopyBlitTag {};
    struct PresentTag {};

    struct ImageTag {};
    struct ImportImageTag {};
    struct ImageViewTag {};
    struct ImportBufferTag {};
    struct BufferTag {};
    struct BufferViewTag {};

    struct RasterSubPass {
        using Tag = RasterSubPassTag;

        PmrHashMap<std::string, RasterView> rasterViews;
        PmrHashMap<std::string, ComputeView> computeViews;
    };

    struct RasterPass {
        using Tag = RasterPassTag;

        PmrHashMap<std::string, RasterView> rasterViews;
        PmrHashMap<std::string, ComputeView> computeViews;
        uint32_t width{0};
        uint32_t height{0};
    };

    struct ComputePass {
        using Tag = ComputePassTag;

        PmrHashMap<std::string, ComputeView> computeViews;
    };

    struct CopyBlitPass {
        using Tag = CopyBlitTag;

        std::vector<CopyView> views;
    };

    struct PresentPass {
        using Tag = CopyBlitTag;

        rhi::SwapChainPtr swapChain;
    };

    struct GraphImportImage {
        using Tag = ImportImageTag;

        rhi::ImagePtr image;
    };

    struct GraphImage {
        using Tag = ImageTag;

        rhi::Extent3D extent = {1, 1, 1};
        uint32_t mipLevel = 1;
        uint32_t arrayLayer = 1;

        rhi::SampleCount sample = rhi::SampleCount::X1;
        rhi::PixelFormat format = rhi::PixelFormat::RGBA8_UNORM;
        rhi::ImageUsageFlags usage = rhi::ImageUsageFlagBit::NONE;
        ResourceResidency residency = ResourceResidency::PERSISTENT;
    };

    struct GraphImageView {
        using Tag = ImageViewTag;

        rhi::ImageViewDesc view;
    };

    struct GraphImportBuffer {
        using Tag = ImportBufferTag;

        rhi::BufferPtr buffer;
    };

    struct GraphBuffer {
        using Tag = BufferTag;

        uint64_t size = 0;
        rhi::BufferUsageFlags usage = rhi::BufferUsageFlagBit::NONE;
        ResourceResidency residency = ResourceResidency::PERSISTENT;
    };

    struct GraphBufferView {
        using Tag = BufferViewTag;

        rhi::BufferViewDesc view;
    };

    using VertexType = uint32_t;
    static constexpr VertexType INVALID_VERTEX = std::numeric_limits<VertexType>::max();

    struct PassGraph {
        using vertex_descriptor = VertexType;
    };

    struct ResourceGraph {
        using vertex_descriptor = VertexType;
        using directed_category = boost::directed_tag;
        using edge_parallel_category = boost::allow_parallel_edge_tag;
        using traversal_category = boost::vertex_list_graph_tag;
        using edge_descriptor = VertexType;

        using OutEdgeList = PmrVector<edge_descriptor>;
        using out_edge_iterator = OutEdgeList::iterator;

        PmrHashMap<VertexType, OutEdgeList> outEdges;
    };

    template <typename Graph>
    struct DirectedGraphHelper : public boost::directed_graph_helper<Graph> {
        using graph_type = Graph;
        Graph &graph;
    };

    struct DependencyGraph {
        using vertex_descriptor = VertexType;
    };

    template <typename Graph>
    void AddEdge(typename Graph::vertex_descriptor u, typename Graph::vertex_descriptor v, Graph &graph)
    {
        SKY_ASSERT(u != INVALID_VERTEX && v != INVALID_VERTEX);
        if constexpr (std::is_same_v<typename Graph::directed_category, boost::directed_tag>) {
            graph.outEdges[u].emplace_back(v);
        }
    }
} // namespace sky