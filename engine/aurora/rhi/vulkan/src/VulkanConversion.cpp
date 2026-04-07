//
// Created on 2026/04/01.
//

#include "VulkanConversion.h"

namespace sky::aurora {

    // ---- PixelFormat ----
    static const VkFormat PIXEL_FORMAT_TABLE[] = {
        VK_FORMAT_UNDEFINED,                  // UNDEFINED
        VK_FORMAT_R8_UINT,                    // R8_UINT
        VK_FORMAT_R8_UNORM,                   // R8_UNORM
        VK_FORMAT_R8_SRGB,                    // R8_SRGB
        VK_FORMAT_R8G8B8A8_UNORM,             // RGBA8_UNORM
        VK_FORMAT_R8G8B8A8_SRGB,              // RGBA8_SRGB
        VK_FORMAT_B8G8R8A8_UNORM,             // BGRA8_UNORM
        VK_FORMAT_B8G8R8A8_SRGB,              // BGRA8_SRGB
        VK_FORMAT_R16_UNORM,                  // R16_UNORM
        VK_FORMAT_R16G16_UNORM,               // RG16_UNORM
        VK_FORMAT_R16G16B16A16_UNORM,         // RGBA16_UNORM
        VK_FORMAT_R16_SFLOAT,                 // R16_SFLOAT
        VK_FORMAT_R16G16_SFLOAT,              // RG16_SFLOAT
        VK_FORMAT_R16G16B16A16_SFLOAT,        // RGBA16_SFLOAT
        VK_FORMAT_R32_SFLOAT,                 // R32_SFLOAT
        VK_FORMAT_R32G32_SFLOAT,              // RG32_SFLOAT
        VK_FORMAT_R32G32B32_SFLOAT,           // RGB32_SFLOAT
        VK_FORMAT_R32G32B32A32_SFLOAT,        // RGBA32_SFLOAT
        VK_FORMAT_R32_UINT,                   // R32_UINT
        VK_FORMAT_R32G32_UINT,                // RG32_UINT
        VK_FORMAT_R32G32B32_UINT,             // RGB32_UINT
        VK_FORMAT_R32G32B32A32_UINT,          // RGBA32_UINT
        VK_FORMAT_D32_SFLOAT,                 // D32
        VK_FORMAT_D24_UNORM_S8_UINT,          // D24_S8
        VK_FORMAT_D32_SFLOAT_S8_UINT,         // D32_S8
        VK_FORMAT_BC1_RGB_UNORM_BLOCK,        // BC1_RGB_UNORM_BLOCK
        VK_FORMAT_BC1_RGB_SRGB_BLOCK,         // BC1_RGB_SRGB_BLOCK
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,       // BC1_RGBA_UNORM_BLOCK
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,        // BC1_RGBA_SRGB_BLOCK
        VK_FORMAT_BC2_UNORM_BLOCK,            // BC2_UNORM_BLOCK
        VK_FORMAT_BC2_SRGB_BLOCK,             // BC2_SRGB_BLOCK
        VK_FORMAT_BC3_UNORM_BLOCK,            // BC3_UNORM_BLOCK
        VK_FORMAT_BC3_SRGB_BLOCK,             // BC3_SRGB_BLOCK
        VK_FORMAT_BC4_UNORM_BLOCK,            // BC4_UNORM_BLOCK
        VK_FORMAT_BC4_SNORM_BLOCK,            // BC4_SNORM_BLOCK
        VK_FORMAT_BC5_UNORM_BLOCK,            // BC5_UNORM_BLOCK
        VK_FORMAT_BC5_SNORM_BLOCK,            // BC5_SNORM_BLOCK
        VK_FORMAT_BC6H_UFLOAT_BLOCK,          // BC6H_UFLOAT_BLOCK
        VK_FORMAT_BC6H_SFLOAT_BLOCK,          // BC6H_SFLOAT_BLOCK
        VK_FORMAT_BC7_UNORM_BLOCK,            // BC7_UNORM_BLOCK
        VK_FORMAT_BC7_SRGB_BLOCK,             // BC7_SRGB_BLOCK
        VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,    // ETC2_R8G8B8_UNORM_BLOCK
        VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,     // ETC2_R8G8B8_SRGB_BLOCK
        VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,  // ETC2_R8G8B8A1_UNORM_BLOCK
        VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,   // ETC2_R8G8B8A1_SRGB_BLOCK
        VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,  // ETC2_R8G8B8A8_UNORM_BLOCK
        VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,   // ETC2_R8G8B8A8_SRGB_BLOCK
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK,       // ASTC_4x4_UNORM_BLOCK
        VK_FORMAT_ASTC_4x4_SRGB_BLOCK,        // ASTC_4x4_SRGB_BLOCK
        VK_FORMAT_ASTC_8x8_UNORM_BLOCK,       // ASTC_8x8_UNORM_BLOCK
        VK_FORMAT_ASTC_8x8_SRGB_BLOCK,        // ASTC_8x8_SRGB_BLOCK
        VK_FORMAT_ASTC_10x10_UNORM_BLOCK,     // ASTC_10x10_UNORM_BLOCK
        VK_FORMAT_ASTC_10x10_SRGB_BLOCK,      // ASTC_10x10_SRGB_BLOCK
        VK_FORMAT_ASTC_12x12_UNORM_BLOCK,     // ASTC_12x12_UNORM_BLOCK
        VK_FORMAT_ASTC_12x12_SRGB_BLOCK,      // ASTC_12x12_SRGB_BLOCK
    };

    VkFormat FromPixelFormat(PixelFormat format)
    {
        const auto idx = static_cast<uint32_t>(format);
        if (idx < static_cast<uint32_t>(PixelFormat::MAX)) {
            return PIXEL_FORMAT_TABLE[idx];
        }
        return VK_FORMAT_UNDEFINED;
    }

    // ---- Format (vertex attribute) ----
    static const VkFormat FORMAT_TABLE[] = {
        VK_FORMAT_UNDEFINED,             // UNDEFINED
        VK_FORMAT_R32_SFLOAT,            // F_R32
        VK_FORMAT_R32G32_SFLOAT,         // F_RG32
        VK_FORMAT_R32G32B32_SFLOAT,      // F_RGB32
        VK_FORMAT_R32G32B32A32_SFLOAT,   // F_RGBA32
        VK_FORMAT_R8_UNORM,              // F_R8
        VK_FORMAT_R8G8_UNORM,            // F_RG8
        VK_FORMAT_R8G8B8_UNORM,          // F_RGB8
        VK_FORMAT_R8G8B8A8_UNORM,        // F_RGBA8
        VK_FORMAT_R8_UINT,               // U_R8
        VK_FORMAT_R8G8_UINT,             // U_RG8
        VK_FORMAT_R8G8B8_UINT,           // U_RGB8
        VK_FORMAT_R8G8B8A8_UINT,         // U_RGBA8
        VK_FORMAT_R16_UINT,              // U_R16
        VK_FORMAT_R16G16_UINT,           // U_RG16
        VK_FORMAT_R16G16B16_UINT,        // U_RGB16
        VK_FORMAT_R16G16B16A16_UINT,     // U_RGBA16
        VK_FORMAT_R32_UINT,              // U_R32
        VK_FORMAT_R32G32_UINT,           // U_RG32
        VK_FORMAT_R32G32B32_UINT,        // U_RGB32
        VK_FORMAT_R32G32B32A32_UINT,     // U_RGBA32
    };

    VkFormat FromFormat(Format format)
    {
        const auto idx = static_cast<uint32_t>(format);
        if (idx < sizeof(FORMAT_TABLE) / sizeof(FORMAT_TABLE[0])) {
            return FORMAT_TABLE[idx];
        }
        return VK_FORMAT_UNDEFINED;
    }

    // ---- SampleCount ----
    VkSampleCountFlagBits FromSampleCount(SampleCount sample)
    {
        return static_cast<VkSampleCountFlagBits>(static_cast<uint32_t>(sample));
    }

    // ---- ImageType ----
    VkImageType FromImageType(ImageType type)
    {
        static const VkImageType TABLE[] = {
            VK_IMAGE_TYPE_1D, // IMAGE_1D
            VK_IMAGE_TYPE_2D, // IMAGE_2D
            VK_IMAGE_TYPE_3D, // IMAGE_3D
        };
        return TABLE[static_cast<uint32_t>(type)];
    }

    // ---- ImageViewType ----
    VkImageViewType FromImageViewType(ImageViewType type)
    {
        static const VkImageViewType TABLE[] = {
            VK_IMAGE_VIEW_TYPE_2D,         // VIEW_2D
            VK_IMAGE_VIEW_TYPE_2D_ARRAY,   // VIEW_2D_ARRAY
            VK_IMAGE_VIEW_TYPE_CUBE,       // VIEW_CUBE
            VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, // VIEW_CUBE_ARRAY
            VK_IMAGE_VIEW_TYPE_3D,         // VIEW_3D
        };
        return TABLE[static_cast<uint32_t>(type)];
    }

    // ---- Filter ----
    VkFilter FromFilter(Filter filter)
    {
        return static_cast<VkFilter>(static_cast<uint32_t>(filter));
    }

    // ---- MipFilter ----
    VkSamplerMipmapMode FromMipFilter(MipFilter filter)
    {
        return static_cast<VkSamplerMipmapMode>(static_cast<uint32_t>(filter));
    }

    // ---- WrapMode ----
    VkSamplerAddressMode FromWrapMode(WrapMode mode)
    {
        static const VkSamplerAddressMode TABLE[] = {
            VK_SAMPLER_ADDRESS_MODE_REPEAT,               // REPEAT
            VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,      // MIRRORED_REPEAT
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,        // CLAMP_TO_EDGE
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,      // CLAMP_TO_BORDER
            VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, // MIRROR_CLAMP_TO_EDGE
        };
        return TABLE[static_cast<uint32_t>(mode)];
    }

    // ---- BlendFactor ----
    VkBlendFactor FromBlendFactor(BlendFactor factor)
    {
        return static_cast<VkBlendFactor>(static_cast<uint32_t>(factor));
    }

    // ---- BlendOp ----
    VkBlendOp FromBlendOp(BlendOp op)
    {
        return static_cast<VkBlendOp>(static_cast<uint32_t>(op));
    }

    // ---- CompareOp ----
    VkCompareOp FromCompareOp(CompareOp op)
    {
        return static_cast<VkCompareOp>(static_cast<uint32_t>(op));
    }

    // ---- StencilOp ----
    VkStencilOp FromStencilOp(StencilOp op)
    {
        return static_cast<VkStencilOp>(static_cast<uint32_t>(op));
    }

    // ---- PrimitiveTopology ----
    VkPrimitiveTopology FromPrimitiveTopology(PrimitiveTopology topo)
    {
        return static_cast<VkPrimitiveTopology>(static_cast<uint32_t>(topo));
    }

    // ---- PolygonMode ----
    VkPolygonMode FromPolygonMode(PolygonMode mode)
    {
        return static_cast<VkPolygonMode>(static_cast<uint32_t>(mode));
    }

    // ---- FrontFace ----
    VkFrontFace FromFrontFace(FrontFace face)
    {
        return static_cast<VkFrontFace>(static_cast<uint32_t>(face));
    }

    // ---- CullMode ----
    VkCullModeFlags FromCullMode(const CullingModeFlags &flags)
    {
        VkCullModeFlags res = VK_CULL_MODE_NONE;
        if (flags & CullModeFlagBits::FRONT) { res |= VK_CULL_MODE_FRONT_BIT; }
        if (flags & CullModeFlagBits::BACK)  { res |= VK_CULL_MODE_BACK_BIT; }
        return res;
    }

    // ---- IndexType ----
    VkIndexType FromIndexType(IndexType type)
    {
        return type == IndexType::U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    }

    // ---- LoadOp / StoreOp ----
    VkAttachmentLoadOp FromLoadOp(LoadOp op)
    {
        static const VkAttachmentLoadOp TABLE[] = {
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, // DONT_CARE
            VK_ATTACHMENT_LOAD_OP_LOAD,      // LOAD
            VK_ATTACHMENT_LOAD_OP_CLEAR,     // CLEAR
        };
        return TABLE[static_cast<uint32_t>(op)];
    }

    VkAttachmentStoreOp FromStoreOp(StoreOp op)
    {
        return op == StoreOp::STORE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    // ---- DescriptorType ----
    VkDescriptorType FromDescriptorType(DescriptorType type)
    {
        static const VkDescriptorType TABLE[] = {
            VK_DESCRIPTOR_TYPE_SAMPLER,                // SAMPLER
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // COMBINED_IMAGE_SAMPLER
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          // SAMPLED_IMAGE
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          // STORAGE_IMAGE
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         // UNIFORM_BUFFER
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         // STORAGE_BUFFER
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, // UNIFORM_BUFFER_DYNAMIC
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, // STORAGE_BUFFER_DYNAMIC
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       // INPUT_ATTACHMENT
        };
        return TABLE[static_cast<uint32_t>(type)];
    }

    // ---- VertexInputRate ----
    VkVertexInputRate FromVertexInputRate(VertexInputRate rate)
    {
        return rate == VertexInputRate::PER_INSTANCE ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
    }

    // ---- ShaderStage ----
    VkShaderStageFlagBits FromShaderStage(ShaderStageFlagBit stage)
    {
        switch (stage) {
        case ShaderStageFlagBit::VS:  return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStageFlagBit::FS:  return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStageFlagBit::CS:  return VK_SHADER_STAGE_COMPUTE_BIT;
#ifndef ANDROID
        case ShaderStageFlagBit::TAS: return VK_SHADER_STAGE_TASK_BIT_EXT;
        case ShaderStageFlagBit::MS:  return VK_SHADER_STAGE_MESH_BIT_EXT;
#endif
        default: return VK_SHADER_STAGE_VERTEX_BIT;
        }
    }

    VkShaderStageFlags FromShaderStageFlags(const ShaderStageFlags &flags)
    {
        VkShaderStageFlags res = 0;
        if (flags & ShaderStageFlagBit::VS)  { res |= VK_SHADER_STAGE_VERTEX_BIT; }
        if (flags & ShaderStageFlagBit::FS)  { res |= VK_SHADER_STAGE_FRAGMENT_BIT; }
        if (flags & ShaderStageFlagBit::CS)  { res |= VK_SHADER_STAGE_COMPUTE_BIT; }
#ifndef ANDROID
        if (flags & ShaderStageFlagBit::TAS) { res |= VK_SHADER_STAGE_TASK_BIT_EXT; }
        if (flags & ShaderStageFlagBit::MS)  { res |= VK_SHADER_STAGE_MESH_BIT_EXT; }
#endif
        return res;
    }

    // ---- ImageUsageFlags ----
    VkImageUsageFlags FromImageUsageFlags(const ImageUsageFlags &flags)
    {
        VkImageUsageFlags res = 0;
        if (flags & ImageUsageFlagBit::TRANSFER_SRC)     { res |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; }
        if (flags & ImageUsageFlagBit::TRANSFER_DST)     { res |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }
        if (flags & ImageUsageFlagBit::SAMPLED)          { res |= VK_IMAGE_USAGE_SAMPLED_BIT; }
        if (flags & ImageUsageFlagBit::STORAGE)          { res |= VK_IMAGE_USAGE_STORAGE_BIT; }
        if (flags & ImageUsageFlagBit::RENDER_TARGET)    { res |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; }
        if (flags & ImageUsageFlagBit::DEPTH_STENCIL)    { res |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; }
        if (flags & ImageUsageFlagBit::TRANSIENT)        { res |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT; }
        if (flags & ImageUsageFlagBit::INPUT_ATTACHMENT)  { res |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT; }
        return res;
    }

    // ---- BufferUsageFlags ----
    VkBufferUsageFlags FromBufferUsageFlags(const BufferUsageFlags &flags)
    {
        VkBufferUsageFlags res = 0;
        if (flags & BufferUsageFlagBit::TRANSFER_SRC) { res |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT; }
        if (flags & BufferUsageFlagBit::TRANSFER_DST) { res |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }
        if (flags & BufferUsageFlagBit::UNIFORM)      { res |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }
        if (flags & BufferUsageFlagBit::STORAGE)      { res |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
        if (flags & BufferUsageFlagBit::VERTEX)       { res |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }
        if (flags & BufferUsageFlagBit::INDEX)        { res |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
        if (flags & BufferUsageFlagBit::INDIRECT)     { res |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT; }
        return res;
    }

    // ---- MemoryType (buffer) ----
    VmaMemoryUsage FromMemoryType(MemoryType type)
    {
        switch (type) {
        case MemoryType::GPU_ONLY:   return VMA_MEMORY_USAGE_GPU_ONLY;
        case MemoryType::CPU_ONLY:   return VMA_MEMORY_USAGE_CPU_ONLY;
        case MemoryType::CPU_TO_GPU: return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case MemoryType::GPU_TO_CPU: return VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:                     return VMA_MEMORY_USAGE_GPU_ONLY;
        }
    }

    // ---- MemoryType (image, with transient check) ----
    VmaMemoryUsage FromMemoryType(MemoryType type, const ImageUsageFlags &usage)
    {
        if ((usage & ImageUsageFlagBit::TRANSIENT) && type == MemoryType::GPU_ONLY) {
            return VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
        }
        return FromMemoryType(type);
    }

    // ---- AspectFlags ----
    VkImageAspectFlags FromAspectFlags(const AspectFlags &flags)
    {
        VkImageAspectFlags res = 0;
        if (flags & AspectFlagBit::COLOR_BIT)   { res |= VK_IMAGE_ASPECT_COLOR_BIT; }
        if (flags & AspectFlagBit::DEPTH_BIT)   { res |= VK_IMAGE_ASPECT_DEPTH_BIT; }
        if (flags & AspectFlagBit::STENCIL_BIT) { res |= VK_IMAGE_ASPECT_STENCIL_BIT; }
        return res;
    }

    // ---- PipelineStageFlags ----
    VkPipelineStageFlags FromPipelineStageFlags(const PipelineStageFlags &flags)
    {
        VkPipelineStageFlags res = 0;
        if (flags & PipelineStageBit::TOP)             { res |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; }
        if (flags & PipelineStageBit::DRAW_INDIRECT)   { res |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT; }
        if (flags & PipelineStageBit::VERTEX_INPUT)    { res |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT; }
        if (flags & PipelineStageBit::VERTEX_SHADER)   { res |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; }
        if (flags & PipelineStageBit::FRAGMENT_SHADER) { res |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
        if (flags & PipelineStageBit::EARLY_FRAGMENT)  { res |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; }
        if (flags & PipelineStageBit::LATE_FRAGMENT)   { res |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; }
        if (flags & PipelineStageBit::COLOR_OUTPUT)    { res |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; }
        if (flags & PipelineStageBit::COMPUTE_SHADER)  { res |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; }
        if (flags & PipelineStageBit::TRANSFER)        { res |= VK_PIPELINE_STAGE_TRANSFER_BIT; }
        if (flags & PipelineStageBit::BOTTOM)          { res |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; }
        return res;
    }

    // ---- StencilState ----
    VkStencilOpState FromStencilState(const StencilState &state)
    {
        VkStencilOpState ret = {};
        ret.failOp      = FromStencilOp(state.failOp);
        ret.passOp      = FromStencilOp(state.passOp);
        ret.depthFailOp = FromStencilOp(state.depthFailOp);
        ret.compareOp   = FromCompareOp(state.compareOp);
        ret.compareMask = state.compareMask;
        ret.writeMask   = state.writeMask;
        ret.reference   = state.reference;
        return ret;
    }

    // ---- QueryType ----
    VkQueryType FromQueryType(QueryType type)
    {
        switch (type) {
        case QueryType::PIPELINE_STATISTICS: return VK_QUERY_TYPE_PIPELINE_STATISTICS;
        case QueryType::TIME_STAMP:          return VK_QUERY_TYPE_TIMESTAMP;
        case QueryType::OCCLUSION:           return VK_QUERY_TYPE_OCCLUSION;
        default: return VK_QUERY_TYPE_MAX_ENUM;
        }
    }

    // ---- QueryPipelineStatisticFlags ----
    VkQueryPipelineStatisticFlags FromPipelineStatisticFlags(const PipelineStatisticFlags &flags)
    {
        VkQueryPipelineStatisticFlags res = 0;
        if (flags & PipelineStatisticFlagBits::IA_VERTICES)      { res |= VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT; }
        if (flags & PipelineStatisticFlagBits::IA_PRIMITIVES)    { res |= VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT; }
        if (flags & PipelineStatisticFlagBits::VS_INVOCATIONS)   { res |= VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT; }
        if (flags & PipelineStatisticFlagBits::FS_INVOCATIONS)   { res |= VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT; }
        if (flags & PipelineStatisticFlagBits::CLIP_INVOCATIONS) { res |= VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT; }
        if (flags & PipelineStatisticFlagBits::CLIP_PRIMITIVES)  { res |= VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT; }
        if (flags & PipelineStatisticFlagBits::CS_INVOCATIONS)   { res |= VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT; }
        return res;
    }

    // ---- DescriptorBindingFlags ----
    VkDescriptorBindingFlags FromDescriptorBindingFlags(const DescriptorBindingFlags &flags)
    {
        VkDescriptorBindingFlags res = 0;
        if (flags & DescriptorBindingFlagBit::VARIABLE_COUNT) { res |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT; }
        return res;
    }

    // ---- ImageSubresourceLayers ----
    VkImageSubresourceLayers FromImageSubRangeLayers(const ImageSubRangeLayers &range)
    {
        return VkImageSubresourceLayers{
            FromAspectFlags(range.aspectMask),
            range.level,
            range.baseLayer,
            range.layers
        };
    }

} // namespace sky::aurora
