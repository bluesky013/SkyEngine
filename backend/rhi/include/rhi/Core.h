//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <core/template/Flags.h>

namespace sky {
    static constexpr uint32_t INVALID_INDEX = ~(0U);
}

namespace sky::rhi {

#define ENABLE_FLAG_BIT_OPERATOR(Type) \
    constexpr Flags<Type> operator&(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) & rhs; \
    } \
    constexpr Flags<Type> operator|(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) | rhs; \
    } \
    constexpr Flags<Type> operator^(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) ^ rhs; \
    }


    enum class PixelFormat : uint32_t {
        UNDEFINED = 0,
        RGB8_UNORM,
        RGB8_SRGB,
        RGBA8_UNORM,
        RGBA8_SRGB,
        BGRA8_UNORM,
        BGRA8_SRGB,
        D32,
        D24_S8,
        D32_S8,
        BC1_RGB_UNORM_BLOCK,
        BC1_RGB_SRGB_BLOCK,
        BC1_RGBA_UNORM_BLOCK,
        BC1_RGBA_SRGB_BLOCK,
        BC2_UNORM_BLOCK,
        BC2_SRGB_BLOCK,
        BC3_UNORM_BLOCK,
        BC3_SRGB_BLOCK,
        BC4_UNORM_BLOCK,
        BC4_SNORM_BLOCK,
        BC5_UNORM_BLOCK,
        BC5_SNORM_BLOCK,
        BC6H_UFLOAT_BLOCK,
        BC6H_SFLOAT_BLOCK,
        BC7_UNORM_BLOCK,
        BC7_SRGB_BLOCK,
        ETC2_R8G8B8_UNORM_BLOCK,
        ETC2_R8G8B8_SRGB_BLOCK,
        ETC2_R8G8B8A1_UNORM_BLOCK,
        ETC2_R8G8B8A1_SRGB_BLOCK,
        ETC2_R8G8B8A8_UNORM_BLOCK,
        ETC2_R8G8B8A8_SRGB_BLOCK,
        ASTC_4x4_UNORM_BLOCK,
        ASTC_4x4_SRGB_BLOCK,
        ASTC_8x8_UNORM_BLOCK,
        ASTC_8x8_SRGB_BLOCK,
        ASTC_10x10_UNORM_BLOCK,
        ASTC_10x10_SRGB_BLOCK,
        ASTC_12x12_UNORM_BLOCK,
        ASTC_12x12_SRGB_BLOCK,
    };

    enum class Format : uint32_t {
        UNDEFINED = 0,
        F_R32     = 1,
        F_RG32    = 2,
        F_RGB32   = 3,
        F_RGBA32  = 4,
        F_R8     = 5,
        F_RG8    = 6,
        F_RGB8   = 7,
        F_RGBA8  = 8
    };

    enum class IndexType : uint32_t {
        U16 = 0,
        U32,
    };

    enum class ImageType : uint32_t {
        IMAGE_2D,
        IMAGE_3D
    };

    enum class ImageViewType : uint32_t {
        VIEW_2D,
        VIEW_2D_ARRAY,
        VIEW_CUBE,
        VIEW_CUBE_ARRAY,
        VIEW_3D,
    };

    enum class MemoryType : uint32_t {
        GPU_ONLY,
        CPU_ONLY,
        CPU_TO_GPU,
        GPU_TO_CPU
    };

    enum class Filter : uint32_t {
        NEAREST = 0,
        LINEAR = 1,
    };

    enum class MipFilter : uint32_t {
        NEAREST = 0,
        LINEAR = 1,
    };

    enum class WrapMode : uint32_t {
        REPEAT               = 0,
        MIRRORED_REPEAT      = 1,
        CLAMP_TO_EDGE        = 2,
        CLAMP_TO_BORDER      = 3,
        MIRROR_CLAMP_TO_EDGE = 4,
    };

    enum class PresentMode : uint32_t {
        IMMEDIATE = 0,
        VSYNC     = 1
    };

    enum class BlendFactor : uint32_t {
        ZERO                     = 0,
        ONE                      = 1,
        SRC_COLOR                = 2,
        ONE_MINUS_SRC_COLOR      = 3,
        DST_COLOR                = 4,
        ONE_MINUS_DST_COLOR      = 5,
        SRC_ALPHA                = 6,
        ONE_MINUS_SRC_ALPHA      = 7,
        DST_ALPHA                = 8,
        ONE_MINUS_DST_ALPHA      = 9,
        CONSTANT_COLOR           = 10,
        ONE_MINUS_CONSTANT_COLOR = 11,
        CONSTANT_ALPHA           = 12,
        ONE_MINUS_CONSTANT_ALPHA = 13,
        SRC_ALPHA_SATURATE       = 14,
        SRC1_COLOR               = 15,
        ONE_MINUS_SRC1_COLOR     = 16,
        SRC1_ALPHA               = 17,
        ONE_MINUS_SRC1_ALPHA     = 18,
    };

    enum class BlendOp : uint32_t {
        ADD      = 0,
        SUBTRACT = 1,
    };

    enum class CompareOp : uint32_t {
        NEVER            = 0,
        LESS             = 1,
        EQUAL            = 2,
        LESS_OR_EQUAL    = 3,
        GREATER          = 4,
        NOT_EQUAL        = 5,
        GREATER_OR_EQUAL = 6,
        ALWAYS           = 7,
    };

    enum class StencilOp : uint32_t {
        KEEP                = 0,
        ZERO                = 1,
        REPLACE             = 2,
        INCREMENT_AND_CLAMP = 3,
        DECREMENT_AND_CLAMP = 4,
        INVERT              = 5,
        INCREMENT_AND_WRAP  = 6,
        DECREMENT_AND_WRAP  = 7,
    };

    enum class SampleCount : uint32_t {
        X1  = 0x00000001,
        X2  = 0x00000002,
        X4  = 0x00000004,
        X8  = 0x00000008,
        X16 = 0x00000010,
        X32 = 0x00000020,
        X64 = 0x00000040,
    };

    enum class PrimitiveTopology : uint32_t {
        POINT_LIST                    = 0,
        LINE_LIST                     = 1,
        LINE_STRIP                    = 2,
        TRIANGLE_LIST                 = 3,
        TRIANGLE_STRIP                = 4,
        TRIANGLE_FAN                  = 5,
    };

    enum class PolygonMode : uint32_t {
        FILL  = 0,
        LINE  = 1,
        POINT = 2,
    };

    enum class FrontFace : uint32_t {
        CW  = 0,
        CCW = 1,
    };

    enum class LoadOp : uint32_t {
        DONT_CARE = 0,
        LOAD  = 1,
        CLEAR = 2
    };

    enum class StoreOp : uint32_t {
        DONT_CARE = 0,
        STORE = 1,
    };

    enum class DescriptorType : uint32_t {
        SAMPLER                = 0,
        COMBINED_IMAGE_SAMPLER = 1,
        SAMPLED_IMAGE          = 2,
        STORAGE_IMAGE          = 3,
        UNIFORM_BUFFER         = 4,
        STORAGE_BUFFER         = 5,
        UNIFORM_BUFFER_DYNAMIC = 6,
        STORAGE_BUFFER_DYNAMIC = 7,
        INPUT_ATTACHMENT       = 8,
    };

    enum class VertexInputRate : uint32_t {
        PER_VERTEX   = 0,
        PER_INSTANCE = 1
    };

    enum class QueueType : uint32_t {
        GRAPHICS,
        TRANSFER,
    };

    enum class SubPassContent : uint32_t {
        INLINE = 0,
        SECONDARY_COMMAND_BUFFERS = 1,
    };

    enum class AccessFlag : uint32_t {
        NONE = 0x00,
        INDIRECT_BUFFER,
        INDEX_BUFFER,
        VERTEX_BUFFER,
        VERTEX_CBV,
        VERTEX_SRV,
        VERTEX_UAV_READ,
        VERTEX_UAV_WRITE,
        FRAGMENT_CBV,
        FRAGMENT_SRV,
        FRAGMENT_UAV_READ,
        FRAGMENT_UAV_WRITE,
        COMPUTE_CBV,
        COMPUTE_SRV,
        COMPUTE_UAV_READ,
        COMPUTE_UAV_WRITE,
        SHADING_RATE,
        COLOR_INPUT,
        DEPTH_STENCIL_INPUT,
        COLOR_INOUT_READ,
        COLOR_INOUT_WRITE,
        DEPTH_STENCIL_INOUT_READ,
        DEPTH_STENCIL_INOUT_WRITE,
        COLOR_READ,
        COLOR_WRITE,
        DEPTH_STENCIL_READ,
        DEPTH_STENCIL_WRITE,
        TRANSFER_READ,
        TRANSFER_WRITE,
        PRESENT,
        GENERAL,
    };

    // flag bit
    enum class ImageUsageFlagBit : uint32_t {
        NONE             = 0x00000000,
        TRANSFER_SRC     = 0x00000001,
        TRANSFER_DST     = 0x00000002,
        SAMPLED          = 0x00000004,
        STORAGE          = 0x00000008,
        RENDER_TARGET    = 0x00000010,
        DEPTH_STENCIL    = 0x00000020,
        TRANSIENT        = 0x00000040,
        INPUT_ATTACHMENT = 0x00000080,
    };
    using ImageUsageFlags = Flags<ImageUsageFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(ImageUsageFlagBit)

    enum class AspectFlagBit : uint32_t {
        COLOR_BIT   = 0x00000001,
        DEPTH_BIT   = 0x00000002,
        STENCIL_BIT = 0x00000004,
    };
    using AspectFlags = Flags<AspectFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(AspectFlagBit)

    enum class BufferUsageFlagBit : uint32_t {
        NONE            = 0x00000000,
        TRANSFER_SRC    = 0x00000001,
        TRANSFER_DST    = 0x00000002,
        UNIFORM         = 0x00000004,
        STORAGE         = 0x00000008,
        VERTEX          = 0x00000010,
        INDEX           = 0x00000020,
        INDIRECT        = 0x00000040,
    };
    using BufferUsageFlags = Flags<BufferUsageFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(BufferUsageFlagBit)

    enum class ShaderStageFlagBit : uint32_t {
        VS = 0x01,
        FS = 0x02,
        CS = 0x04,
    };
    using ShaderStageFlags = Flags<ShaderStageFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(ShaderStageFlagBit)

    enum class CullModeFlagBits : uint32_t {
        NONE  = 0x00000000,
        FRONT = 0x00000001,
        BACK  = 0x00000002,
    };
    using CullingModeFlags = Flags<CullModeFlagBits>;
    ENABLE_FLAG_BIT_OPERATOR(CullModeFlagBits)

    enum class PipelineStageBit : uint32_t {
        NONE            = 0x00000000,
        TOP             = 0x00000001,
        DRAW_INDIRECT   = 0x00000002,
        VERTEX_INPUT    = 0x00000004,
        VERTEX_SHADER   = 0x00000008,
        FRAGMENT_SHADER = 0x00000010,
        EARLY_FRAGMENT  = 0x00000020,
        LATE_FRAGMENT   = 0x00000040,
        COLOR_OUTPUT    = 0x00000080,
        COMPUTE_SHADER  = 0x00000100,
        TRANSFER        = 0x00000200,
        BOTTOM          = 0x00000400
    };
    using PipelineStageFlags = Flags<PipelineStageBit>;
    ENABLE_FLAG_BIT_OPERATOR(PipelineStageBit)

    enum class DescriptorBindFlagBit : uint32_t {
        NONE          = 0x00000000,
        FEEDBACK_LOOP = 0x00000001
    };
    using DescriptorBindFlags = Flags<DescriptorBindFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(DescriptorBindFlagBit)

    // structs
    struct Offset2D {
        int32_t x;
        int32_t y;
    };

    struct Offset3D {
        int32_t x;
        int32_t y;
        int32_t z;
    };

    struct Extent2D {
        uint32_t width;
        uint32_t height;
    };

    struct Extent3D {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };

    struct Rect2D {
        Offset2D offset;
        Extent2D extent;
    };

    struct Viewport {
        float x = 0.f;
        float y = 0.f;
        float width = 1.f;
        float height = 1.f;
        float minDepth = 0.f;
        float maxDepth = 1.f;
    };

    union ClearColorValue {
        float       float32[4];
        int32_t     int32[4];
        uint32_t    uint32[4];
    };

    struct ClearDepthStencilValue {
        float    depth;
        uint32_t stencil;
    };

    union ClearValue {
        ClearValue() noexcept = default;
        ClearValue(float r, float g, float b, float a) noexcept
        {
            color.float32[0] = r;
            color.float32[1] = g;
            color.float32[2] = b;
            color.float32[3] = a;
        }

        ClearValue(float d, uint32_t s) noexcept
        {
            depthStencil.depth = d;
            depthStencil.stencil = s;
        }

        ClearColorValue        color;
        ClearDepthStencilValue depthStencil;
    };

    struct BufferProvider {
        BufferProvider() = default;
        virtual ~BufferProvider() = default;
        virtual const uint8_t* GetData(uint64_t offset) const = 0;
    };

    struct BufferUploadRequest {
        std::shared_ptr<BufferProvider> source;
        uint64_t       offset = 0;
        uint64_t       size   = 0;
    };

    struct ImageUploadRequest {
        const uint8_t *data     = nullptr;
        uint64_t       offset   = 0;
        uint64_t       size     = 0;
        uint32_t       mipLevel = 0;
        uint32_t       layer    = 0;
        Offset3D       imageOffset;
        Extent3D       imageExtent;
    };

    struct ImageFormatInfo {
        uint32_t blockSize   = 4;
        uint32_t blockWidth  = 1;
        uint32_t blockHeight = 1;
        bool isCompressed    = false;
    };

    struct ImageSubRange {
        uint32_t baseLevel = 0;
        uint32_t levels    = 1;
        uint32_t baseLayer = 0;
        uint32_t layers    = 1;
    };

    struct StencilState {
        StencilOp failOp = StencilOp::KEEP;
        StencilOp passOp = StencilOp::KEEP;
        StencilOp depthFailOp = StencilOp::KEEP;
        CompareOp compareOp   = CompareOp::NEVER;
        uint32_t  compareMask = 0;
        uint32_t  writeMask   = 0;
        uint32_t  reference   = 0;
    };

    struct DepthStencil {
        bool depthTest      = false;
        bool depthWrite     = false;
        bool stencilTest    = false;
        CompareOp compareOp = CompareOp::LESS_OR_EQUAL;
        float minDepth      = 0.f;
        float maxDepth      = 1.f;
        StencilState        front;
        StencilState        back;
    };

    struct BlendState {
        bool blendEn         = false;
        uint8_t writeMask    = 0xF;
        uint8_t padding[2]   = {0};
        BlendFactor srcColor = BlendFactor::ZERO;
        BlendFactor dstColor = BlendFactor::ZERO;
        BlendFactor srcAlpha = BlendFactor::ZERO;
        BlendFactor dstAlpha = BlendFactor::ZERO;
        BlendOp colorBlendOp = BlendOp::ADD;
        BlendOp alphaBlendOp = BlendOp::ADD;
    };

    struct RasterState {
        bool             depthClampEnable        = false;
        bool             rasterizerDiscardEnable = false;
        bool             depthBiasEnable         = false;
        float            depthBiasConstantFactor = 0.f;
        float            depthBiasClamp          = 0.f;
        float            depthBiasSlopeFactor    = 0.f;
        float            lineWidth               = 1.f;
        CullingModeFlags cullMode                = CullModeFlagBits::NONE;
        FrontFace        frontFace   = FrontFace::CCW;
        PolygonMode      polygonMode = PolygonMode::FILL;
    };

    struct InputAssembly {
        PrimitiveTopology topology = PrimitiveTopology::TRIANGLE_LIST;
    };

    struct MultiSample {
        SampleCount sampleCount = SampleCount::X1;
    };

    struct PipelineState {
        DepthStencil            depthStencil;
        MultiSample             multiSample;
        InputAssembly           inputAssembly;
        RasterState             rasterState;
        std::vector<BlendState> blendStates;
    };

    struct VertexAttributeDesc {
        uint32_t location = 0;
        uint32_t binding  = 0;
        uint32_t offset   = 0;
        Format   format   = Format::UNDEFINED;
    };

    struct VertexBindingDesc {
        uint32_t        binding   = 0;
        uint32_t        stride    = 0;
        VertexInputRate inputRate = VertexInputRate::PER_VERTEX;
    };

    struct ImageViewDesc {
        ImageSubRange subRange = {0, 1, 0, 1};
        AspectFlags   mask = rhi::AspectFlagBit::COLOR_BIT;
        ImageViewType viewType = ImageViewType::VIEW_2D;
    };

    struct BufferViewDesc {
        uint64_t offset = 0;
        uint64_t range  = 0;
        PixelFormat format = PixelFormat::UNDEFINED;
    };
} // namespace sky
