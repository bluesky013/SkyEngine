//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <limits>
#include <boost/graph/adjacency_list.hpp>
#include <core/std/Container.h>
#include <rhi/Device.h>
#include <render/SceneView.h>
#include <render/RenderPrimitive.h>
#include <render/resource/ResourceGroup.h>

namespace sky::rdg {

    enum class ResourceResidency {
        TRANSIENT,
        PERSISTENT,
    };

    enum class RasterTypeBit {
        NONE           = 0x00,
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

    enum class PresentType {
        PRESENT
    };
    using AttachmentType = std::variant<RasterType, ComputeType, TransferType, PresentType>;

    enum class ResourceAccessBit : uint32_t {
        READ = 0x01,
        WRITE = 0x02,
        READ_WRITE = READ | WRITE
    };
    using ResourceAccess = Flags<ResourceAccessBit>;
    ENABLE_FLAG_BIT_OPERATOR(ResourceAccessBit)

    using VertexType = uint32_t;
    static constexpr VertexType INVALID_VERTEX = std::numeric_limits<VertexType>::max();
    static constexpr uint8_t INVALID_VIEW_INDEX = std::numeric_limits<uint8_t>::max();
    using VertexList = PmrVector<VertexType>;

    struct RootTag {};

    struct RasterPassTag {};
    struct RasterSubPassTag {};
    struct RasterQueueTag {};
    struct SceneViewTag {};
    struct FullScreenBlitTag {};
    struct ComputePassTag {};
    struct CopyBlitTag {};
    struct PresentTag {};
    struct UploadTag {};
    struct TransitionTag {};
    using RenderGraphTags = std::variant<RootTag, RasterPassTag, RasterSubPassTag, ComputePassTag, CopyBlitTag, PresentTag, SceneViewTag, RasterQueueTag, FullScreenBlitTag, UploadTag, TransitionTag>;

    struct ImageTag {};
    struct ImportImageTag {};
    struct ImportImageViewTag {};
    struct ImageViewTag {};
    struct ImportSwapChainTag {};
    struct ImportXRSwapChainTag {};
    struct BufferTag {};
    struct ImportBufferTag {};
    struct ConstantBufferTag {};
    struct BufferViewTag {};
    struct SamplerTag {};
    using ResourceGraphTags = std::variant<RootTag, ImageTag, ImageViewTag, ImportImageTag, ImportImageViewTag, ImportSwapChainTag, ImportXRSwapChainTag, BufferTag, ImportBufferTag, BufferViewTag, ConstantBufferTag, SamplerTag>;

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
    };

    struct AccessPass {
        using Tag = AccessPassTag;
        VertexType vertexID = INVALID_VERTEX;
    };

    struct AccessRes {
        using Tag = AccessResTag;
        VertexType resID = INVALID_VERTEX;
        VertexType inAccessPassID = INVALID_VERTEX;
        VertexType nextAccessResID = INVALID_VERTEX;
        AccessRange subRange;
        rhi::AccessFlags access;
    };

    struct LifeTime {
        VertexType begin = INVALID_VERTEX;
        VertexType end   = 0;
        uint32_t reference = 0;
    };

    struct RasterAttachment {
        Name name;
        rhi::LoadOp  loadOp  = rhi::LoadOp::DONT_CARE;
        rhi::StoreOp storeOp = rhi::StoreOp::DONT_CARE;
        rhi::LoadOp  stencilLoad = rhi::LoadOp::DONT_CARE;
        rhi::StoreOp stencilStore = rhi::StoreOp::DONT_CARE;
    };

    struct RasterAttachmentRef {
        Name name;
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
        AccessRange range;
        uint32_t srcQueueFamily = (~0U);
        uint32_t dstQueueFamily = (~0U);
    };

    struct RasterView {
        RasterType type = RasterTypeBit::COLOR;
        ResourceAccess access = ResourceAccessBit::READ;
    };

    struct ComputeView {
        Name name;
        ComputeType type = ComputeType::UAV;
        rhi::ShaderStageFlags visibility;
        ResourceAccess access = ResourceAccessBit::READ;
    };

    struct CopyView {
        Name srcName;
        rhi::ImageSubRangeLayers srcRange;
        rhi::Extent3D srcExtent;
        rhi::Offset3D srcOffset;
        Name dstName;
        rhi::ImageSubRangeLayers dstRange;
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
        uint32_t viewMask = 0;
        VertexType parent = INVALID_VERTEX;
        PmrVector<RasterAttachmentRef> colors;
        PmrVector<RasterAttachmentRef> resolves;
        PmrVector<RasterAttachmentRef> inputs;
        RasterAttachmentRef depthStencil;
        PmrHashMap<Name, RasterView> rasterViews;
        PmrHashMap<Name, ComputeView> computeViews;
    };

    struct FullScreenBlit {
        explicit FullScreenBlit(PmrResource *res, uint32_t pass) : passID(pass) {}
        using Tag = FullScreenBlitTag;

        RDGfxTechPtr technique;
        RDProgramPtr program;
        uint32_t passID;
        ResourceGroup *resourceGroup = nullptr;
        rhi::GraphicsPipelinePtr pso;
    };

    struct RasterQueue {
        explicit RasterQueue(PmrResource *res, uint32_t pass)
            : passID(pass)
            , renderItems(res)
            , sort(false)
        {}

        using Tag = RasterQueueTag;

        uint32_t passID;
        uint32_t viewID = INVALID_VERTEX;
        Name rasterID;   // invalid id
        PmrList<RenderItem> renderItems;
        RDResourceLayoutPtr layout;
        ResourceGroup *resourceGroup = nullptr;

        bool sort;
    };

    struct RasterPass {
        RasterPass(uint32_t w, uint32_t h, PmrResource *res)
            : width(w)
            , height(h)
            , attachments(res)
            , attachmentVertex(res)
            , subPasses(res)
            , correlationMasks{res}
            , clearValues(res)
            , dependencies(res)
            , frontBarriers(res)
            , rearBarriers(res)
            {}

        using Tag = RasterPassTag;
        uint32_t width{0};
        uint32_t height{0};
        ShaderVariantKey pipelineKey;

        PmrVector<RasterAttachment> attachments;
        PmrVector<VertexType> attachmentVertex;
        PmrVector<VertexType> subPasses;
        PmrVector<uint32_t> correlationMasks;
        PmrVector<rhi::ClearValue> clearValues;
        PmrVector<SubPassDependency> dependencies;
        PmrHashMap<VertexType, std::vector<GraphBarrier>> frontBarriers; // key resID
        PmrHashMap<VertexType, std::vector<GraphBarrier>> rearBarriers;  // key resID
        RDGfxTechPtr technique;
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
        PmrHashMap<Name, ComputeView> computeViews;
        PmrHashMap<VertexType, std::vector<GraphBarrier>> frontBarriers; // key resID
        PmrHashMap<VertexType, std::vector<GraphBarrier>> rearBarriers;  // key resID

        RDResourceLayoutPtr layout;
        ResourceGroup *resourceGroup = nullptr;
    };

    struct CopyBlitPass {
        explicit CopyBlitPass(PmrResource *res)
            : src(INVALID_VERTEX)
            , dst(INVALID_VERTEX)
            , srcExt{1, 1, 1}
            , dstExt{1, 1, 1}
            , frontBarriers(res)
            , rearBarriers(res)
            {}

        using Tag = CopyBlitTag;
        VertexType src;
        VertexType dst;
        rhi::Extent3D srcExt;
        rhi::Extent3D dstExt;
        rhi::ImageSubRangeLayers srcRange;
        rhi::ImageSubRangeLayers dstRange;
        PmrHashMap<VertexType, std::vector<GraphBarrier>> frontBarriers; // key resID
        PmrHashMap<VertexType, std::vector<GraphBarrier>> rearBarriers;  // key resID
    };

    struct RdgSceneView {
        using Tag = SceneViewTag;
        SceneView* sceneView = nullptr;
    };

    struct TransitionPass {
        explicit TransitionPass(VertexType vtx, PmrResource *res)
            : resID(vtx), frontBarriers(res)
        {}

        using Tag = TransitionTag;

        VertexType resID;
        PmrHashMap<VertexType, std::vector<GraphBarrier>> frontBarriers;  // key resID
        PmrHashMap<VertexType, std::vector<GraphBarrier>> rearBarriers;  // key resID
    };

    struct PresentPass {
        explicit PresentPass(VertexType resID, const rhi::SwapChainPtr &swc, PmrResource *res)
            : imageID(resID)
            , swapChain(swc)
            , frontBarriers(res)
            , rearBarriers(res)
        {}
#ifdef SKY_ENABLE_XR
        explicit PresentPass(VertexType resID, const rhi::XRSwapChainPtr &swc, PmrResource *res)
                : imageID(resID)
                , xrSwapChain(swc)
                , frontBarriers(res)
                , rearBarriers(res)
        {}
#endif
        using Tag = PresentTag;

        VertexType imageID = INVALID_VERTEX;
        rhi::SwapChainPtr swapChain;
#ifdef SKY_ENABLE_XR
        rhi::XRSwapChainPtr xrSwapChain;
#endif
        PmrHashMap<VertexType, std::vector<GraphBarrier>> frontBarriers; // key resID
        PmrHashMap<VertexType, std::vector<GraphBarrier>> rearBarriers;  // key resID
    };

    struct UploadPass {
        using Tag = UploadTag;

        RDBufferPtr src;
        RDBufferPtr dst;
        uint64_t srcOffset = 0;
        uint64_t dstOffset = 0;
        uint64_t copySize = 0;
    };

    struct GraphImportImage {
        using Tag = ImportImageTag;

        rhi::ImagePtr      image;
        rhi::ImageViewType viewType = rhi::ImageViewType::VIEW_2D;
        rhi::AccessFlags   importFlags = rhi::AccessFlagBit::NONE;
    };

    struct GraphImportImageView {
        using Tag = ImportImageViewTag;

        rhi::ImagePtr     image;
        rhi::ImageViewPtr view;
        rhi::AccessFlags  importFlags = rhi::AccessFlagBit::NONE;
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

        rhi::ImagePtr      image;
    };

    struct GraphSwapChain {
        using Tag = ImportSwapChainTag;

        rhi::SwapChainPtr swapchain;
        uint32_t imageIndex = 0;
    };


    struct GraphXRSwapChain {
        using Tag = ImportXRSwapChainTag;

#ifdef SKY_ENABLE_XR
        rhi::XRSwapChainPtr swapchain;
        uint32_t imageIndex = 0;
#endif
    };

    struct GraphImageView {
        using Tag = ImageViewTag;

        rhi::ImageViewDesc view;
    };

    struct GraphImportBuffer {
        using Tag = ImportBufferTag;

        rhi::BufferPtr buffer;
        rhi::AccessFlags importFlags = rhi::AccessFlagBit::NONE;
    };

    struct GraphConstantBuffer {
        using Tag = ConstantBufferTag;

        RDUniformBufferPtr ubo;
    };

    struct GraphSampler {
        using Tag = SamplerTag;

        rhi::SamplerPtr sampler;
    };

    struct GraphBuffer {
        using Tag = BufferTag;

        uint64_t size = 0;
        rhi::BufferUsageFlags usage = rhi::BufferUsageFlagBit::NONE;
        ResourceResidency residency = ResourceResidency::PERSISTENT;
        rhi::BufferPtr buffer;
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
    };

    template <typename T>
    struct BufferViewRes {
        explicit BufferViewRes(const T &v) : desc(v) {}

        T desc;
        LifeTime lifeTime;
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
