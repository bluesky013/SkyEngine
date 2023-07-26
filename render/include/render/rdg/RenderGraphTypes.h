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
#include <rhi/RenderPass.h>
#include <rhi/FrameBuffer.h>
#include <rhi/Swapchain.h>
#include <rhi/ComputePipeline.h>

namespace sky::rdg {

    enum class ResourceResidency {
        TRANSIENT,
        PERSISTENT,
    };

    enum class RasterTypeBit {
        COLOR          = 0x01,
        RESOLVE        = 0x02,
        INPUT          = 0x04,
        DEPTH_STENCIL  = 0x08,
        SHADING_RATE   = 0x10,
    };
    using RasterType = Flags<RasterTypeBit>;
    ENABLE_FLAG_BIT_OPERATOR(RasterTypeBit)

    enum class ComputeType {
        CBV = 0,
        SRV,
        UAV,
    };

    enum class TransferType {
        SRC,
        DST,
    };
    using AttachmentType = std::variant<RasterType, ComputeType, TransferType>;

    enum class ResourceAccessBit : uint32_t {
        READ = 0x01,
        WRITE = 0x02,
        READ_WRITE = READ | WRITE
    };
    using ResourceAccess = Flags<ResourceAccessBit>;
    ENABLE_FLAG_BIT_OPERATOR(ResourceAccessBit)

    using VertexType = uint32_t;
    static constexpr VertexType INVALID_VERTEX = std::numeric_limits<VertexType>::max();
    using VertexList = PmrVector<VertexType>;

    struct RootTag {};

    struct RasterPassTag {};
    struct RasterSubPassTag {};
    struct ComputePassTag {};
    struct CopyBlitTag {};
    struct PresentTag {};
    using RenderGraphTags = std::variant<RootTag, RasterPassTag, RasterSubPassTag, ComputePassTag, CopyBlitTag, PresentTag>;

    struct ImageTag {};
    struct ImportImageTag {};
    struct ImageViewTag {};
    struct BufferTag {};
    struct ImportBufferTag {};
    struct BufferViewTag {};
    using ResourceGraphTags = std::variant<RootTag, ImageTag, ImageViewTag, ImportImageTag, BufferTag, ImportBufferTag, BufferViewTag>;

    struct AccessPassTag {};
    struct AccessResTag {};
    using AccessGraphTags = std::variant<AccessPassTag, AccessResTag>;

    struct DependencyInfo {
        AttachmentType type;
        ResourceAccess access;
        rhi::ShaderStageFlags visibility;
    };

    /*
     * Buffer: [base range]
     * Image: [level, levelCount]
     */
    struct AccessRange {
        uint64_t base   = 0;
        uint64_t range  = 0;
        uint32_t layer  = 0;
        uint32_t layers = 0;
        rhi::AspectFlags aspectMask;
    };

    struct AccessEdge {
        DependencyInfo dependencyInfo;
        AccessRange range;
    };

    struct AccessPass {
        using Tag = AccessPassTag;
        VertexType vertexID = INVALID_VERTEX;
    };

    struct AccessRes {
        using Tag = AccessResTag;
        VertexType resID = INVALID_VERTEX;
        AccessRange subRange;
        rhi::ImageLayout layout = rhi::ImageLayout::UNDEFINED;
        bool visited = false;
    };

    struct LifeTime {
        VertexType begin = INVALID_VERTEX;
        VertexType end   = 0;
        uint32_t reference = 0;
    };

    struct RasterAttachment {
        std::string name;
        rhi::LoadOp  loadOp  = rhi::LoadOp::DONT_CARE;
        rhi::StoreOp storeOp = rhi::StoreOp::DONT_CARE;
        rhi::LoadOp  stencilLoad = rhi::LoadOp::DONT_CARE;
        rhi::StoreOp stencilStore = rhi::StoreOp::DONT_CARE;
    };

    struct RasterAttachmentRef {
        std::string name;
        ResourceAccess access = ResourceAccessBit::READ;
        uint32_t index = INVALID_INDEX;
    };

    struct SubPassDependency {
        uint32_t src = INVALID_VERTEX;
        uint32_t dst = INVALID_VERTEX;
        rhi::AccessFlags preAccess;
        rhi::AccessFlags nextAccess;
    };

    struct GraphBarrier {
        rhi::AccessFlags srcFlags;
        rhi::AccessFlags dstFlags;
        uint32_t srcQueueFamily = (~0U);
        uint32_t dstQueueFamily = (~0U);
        AccessRange range;
    };

    struct RasterView {
        RasterType type = RasterTypeBit::COLOR;
        ResourceAccess access = ResourceAccessBit::READ;
    };

    struct ComputeView {
        std::string name;
        ComputeType type = ComputeType::UAV;
        ResourceAccess access = ResourceAccessBit::READ;
        rhi::ShaderStageFlags visibility;
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

    struct Root {
        using Tag = RootTag;
    };

    struct RasterSubPass {
        explicit RasterSubPass(PmrResource *res) : colors(res), resolves(res), inputs(res), rasterViews(res), computeViews(res) {}

        using Tag = RasterSubPassTag;

        uint32_t subPassID = 0;
        VertexType parent = INVALID_VERTEX;
        PmrVector<RasterAttachmentRef> colors;
        PmrVector<RasterAttachmentRef> resolves;
        PmrVector<RasterAttachmentRef> inputs;
        RasterAttachmentRef depthStencil;
        PmrHashMap<std::string, RasterView> rasterViews;
        PmrHashMap<std::string, ComputeView> computeViews;
    };

    struct RasterPass {
        RasterPass(uint32_t w, uint32_t h, PmrResource *res)
            : width(w)
            , height(h)
            , attachments(res)
            , attachmentVertex(res)
            , subPasses(res)
            , clearValues(res)
            , dependencies(res)
            , frontBarriers(res)
            , rearBarriers(res)
            {}

        using Tag = RasterPassTag;
        uint32_t width{0};
        uint32_t height{0};

        PmrVector<RasterAttachment> attachments;
        PmrVector<VertexType> attachmentVertex;
        PmrVector<VertexType> subPasses;
        PmrVector<rhi::ClearValue> clearValues;
        PmrVector<SubPassDependency> dependencies;
        PmrHashMap<VertexType, GraphBarrier> frontBarriers; // key resID
        PmrHashMap<VertexType, GraphBarrier> rearBarriers;  // key resID

        rhi::RenderPassPtr renderPass;
        rhi::FrameBufferPtr frameBuffer;
    };

    struct ComputePass {
        explicit ComputePass(PmrResource *res)
            : computeViews(res)
            , frontBarriers(res)
            , rearBarriers(res)
            {}

        using Tag = ComputePassTag;
        PmrHashMap<std::string, ComputeView> computeViews;
        PmrVector<GraphBarrier> frontBarriers;
        PmrVector<GraphBarrier> rearBarriers;
    };

    struct CopyBlitPass {
        explicit CopyBlitPass(PmrResource *res)
            : views(res)
            , frontBarriers(res)
            , rearBarriers(res)
            {}

        using Tag = CopyBlitTag;
        PmrVector<CopyView> views;
        PmrVector<GraphBarrier> frontBarriers;
        PmrVector<GraphBarrier> rearBarriers;
    };

    struct PresentPass {
        using Tag = CopyBlitTag;

        rhi::SwapChainPtr swapChain;
    };

    struct GraphImportImage {
        using Tag = ImportImageTag;

        rhi::ImagePtr      image;
        rhi::ImageViewType viewType = rhi::ImageViewType::VIEW_2D;
    };

    struct GraphImage {
        using Tag = ImageTag;

        rhi::Extent3D        extent      = {1, 1, 1};
        uint32_t             mipLevels   = 1;
        uint32_t             arrayLayers = 1;
        rhi::PixelFormat     format      = rhi::PixelFormat::RGBA8_UNORM;
        rhi::ImageUsageFlags usage       = rhi::ImageUsageFlagBit::NONE;
        rhi::SampleCount     samples     = rhi::SampleCount::X1;
        rhi::ImageViewType   viewType    = rhi::ImageViewType::VIEW_2D;
        ResourceResidency    residency   = ResourceResidency::TRANSIENT;
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

    template <class Key>
    struct ColorMap : public boost::put_get_helper<boost::default_color_type &, ColorMap<Key>> {
        using value_type = boost::default_color_type;
        using reference = boost::default_color_type &;
        using key_type = Key;
        using category = boost::lvalue_property_map_tag;

        explicit ColorMap(PmrVector<Key> &vec) noexcept
            : container{vec} {}

        inline reference operator[](const size_t &v) const noexcept {
            return container[v];
        }
        inline reference operator()(const size_t &v) const noexcept {
            return operator[](v);
        }

        PmrVector<Key> &container;
    };

    template <class T>
    ColorMap(PmrVector<T>&) -> ColorMap<T>;

    template <typename T>
    struct ImageViewRes {
        explicit ImageViewRes(const T &v) : desc(v) {}

        T desc;
        LifeTime lifeTime;
        rhi::ImageViewPtr res;
        rhi::AccessFlags  lastUsage = rhi::AccessFlagBit::NONE; // for persistent image
    };

    template <typename T>
    struct BufferViewRes {
        explicit BufferViewRes(const T &v) : desc(v) {}

        T desc;
        LifeTime lifeTime;
        rhi::BufferViewPtr res;
        rhi::AccessFlags   lastUsage = rhi::AccessFlagBit::NONE; // for persistent image
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
