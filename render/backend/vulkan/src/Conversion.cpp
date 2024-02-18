//
// Created by Zach Lee on 2022/11/10.
//

#include <vulkan/Conversion.h>
#include <unordered_map>

namespace sky::vk {
    std::unordered_map<rhi::PixelFormat, VkFormat> PIXEL_FORMAT_TABLE = {
        {rhi::PixelFormat::UNDEFINED,                 VK_FORMAT_UNDEFINED},
        {rhi::PixelFormat::R8_UNORM,                  VK_FORMAT_R8_UNORM},
        {rhi::PixelFormat::R8_SRGB,                   VK_FORMAT_R8_SRGB},
        {rhi::PixelFormat::RGBA8_UNORM,               VK_FORMAT_R8G8B8A8_UNORM},
        {rhi::PixelFormat::RGBA8_SRGB,                VK_FORMAT_R8G8B8A8_SRGB},
        {rhi::PixelFormat::BGRA8_UNORM,               VK_FORMAT_B8G8R8A8_UNORM},
        {rhi::PixelFormat::BGRA8_SRGB,                VK_FORMAT_B8G8R8A8_SRGB},
        {rhi::PixelFormat::R16_UNORM,                 VK_FORMAT_R16_UNORM},
        {rhi::PixelFormat::RGBA16_SFLOAT,             VK_FORMAT_R16G16B16A16_SFLOAT},
        {rhi::PixelFormat::R32_SFLOAT,                VK_FORMAT_R32_SFLOAT},
        {rhi::PixelFormat::D32,                       VK_FORMAT_D32_SFLOAT},
        {rhi::PixelFormat::D24_S8,                    VK_FORMAT_D24_UNORM_S8_UINT},
        {rhi::PixelFormat::D32_S8,                    VK_FORMAT_D32_SFLOAT_S8_UINT},
        {rhi::PixelFormat::BC1_RGB_UNORM_BLOCK,       VK_FORMAT_BC1_RGB_UNORM_BLOCK},
        {rhi::PixelFormat::BC1_RGB_SRGB_BLOCK,        VK_FORMAT_BC1_RGB_SRGB_BLOCK},
        {rhi::PixelFormat::BC1_RGBA_UNORM_BLOCK,      VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
        {rhi::PixelFormat::BC1_RGBA_SRGB_BLOCK,       VK_FORMAT_BC1_RGBA_SRGB_BLOCK},
        {rhi::PixelFormat::BC2_UNORM_BLOCK,           VK_FORMAT_BC2_UNORM_BLOCK},
        {rhi::PixelFormat::BC2_SRGB_BLOCK,            VK_FORMAT_BC2_SRGB_BLOCK},
        {rhi::PixelFormat::BC3_UNORM_BLOCK,           VK_FORMAT_BC3_UNORM_BLOCK},
        {rhi::PixelFormat::BC3_SRGB_BLOCK,            VK_FORMAT_BC3_SRGB_BLOCK},
        {rhi::PixelFormat::BC4_UNORM_BLOCK,           VK_FORMAT_BC4_UNORM_BLOCK},
        {rhi::PixelFormat::BC4_SNORM_BLOCK,           VK_FORMAT_BC4_SNORM_BLOCK},
        {rhi::PixelFormat::BC5_UNORM_BLOCK,           VK_FORMAT_BC5_UNORM_BLOCK},
        {rhi::PixelFormat::BC5_SNORM_BLOCK,           VK_FORMAT_BC5_SNORM_BLOCK},
        {rhi::PixelFormat::BC6H_UFLOAT_BLOCK,         VK_FORMAT_BC6H_UFLOAT_BLOCK},
        {rhi::PixelFormat::BC6H_SFLOAT_BLOCK,         VK_FORMAT_BC6H_SFLOAT_BLOCK},
        {rhi::PixelFormat::BC7_UNORM_BLOCK,           VK_FORMAT_BC7_UNORM_BLOCK},
        {rhi::PixelFormat::BC7_SRGB_BLOCK,            VK_FORMAT_BC7_SRGB_BLOCK},
        {rhi::PixelFormat::ETC2_R8G8B8_UNORM_BLOCK,   VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK},
        {rhi::PixelFormat::ETC2_R8G8B8_SRGB_BLOCK,    VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK},
        {rhi::PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK},
        {rhi::PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK,  VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK},
        {rhi::PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK},
        {rhi::PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK,  VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK},
        {rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK,      VK_FORMAT_ASTC_4x4_UNORM_BLOCK},
        {rhi::PixelFormat::ASTC_4x4_SRGB_BLOCK,       VK_FORMAT_ASTC_4x4_SRGB_BLOCK},
        {rhi::PixelFormat::ASTC_8x8_UNORM_BLOCK,      VK_FORMAT_ASTC_8x8_UNORM_BLOCK},
        {rhi::PixelFormat::ASTC_8x8_SRGB_BLOCK,       VK_FORMAT_ASTC_8x8_SRGB_BLOCK},
        {rhi::PixelFormat::ASTC_10x10_UNORM_BLOCK,    VK_FORMAT_ASTC_10x10_UNORM_BLOCK},
        {rhi::PixelFormat::ASTC_10x10_SRGB_BLOCK,     VK_FORMAT_ASTC_10x10_SRGB_BLOCK},
        {rhi::PixelFormat::ASTC_12x12_UNORM_BLOCK,    VK_FORMAT_ASTC_12x12_UNORM_BLOCK},
        {rhi::PixelFormat::ASTC_12x12_SRGB_BLOCK,     VK_FORMAT_ASTC_12x12_SRGB_BLOCK},
    };

    std::unordered_map<rhi::Format, VkFormat> FORMAT_TABLE = {
        {rhi::Format::UNDEFINED, VK_FORMAT_UNDEFINED},
        {rhi::Format::F_R32, VK_FORMAT_R32_SFLOAT},
        {rhi::Format::F_RG32, VK_FORMAT_R32G32_SFLOAT},
        {rhi::Format::F_RGB32, VK_FORMAT_R32G32B32_SFLOAT},
        {rhi::Format::F_RGBA32, VK_FORMAT_R32G32B32A32_SFLOAT},
        {rhi::Format::F_R8, VK_FORMAT_R8_UNORM},
        {rhi::Format::F_RG8, VK_FORMAT_R8G8_UNORM},
        {rhi::Format::F_RGBA8, VK_FORMAT_R8G8B8A8_UNORM},
    };

    std::unordered_map<rhi::WrapMode, VkSamplerAddressMode> SAMPLER_ADDRESS_TABLE = {
        {rhi::WrapMode::REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT},
        {rhi::WrapMode::MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT},
        {rhi::WrapMode::CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
        {rhi::WrapMode::CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER},
        {rhi::WrapMode::MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE},
    };

    std::unordered_map<VkFormatFeatureFlagBits, rhi::PixelFormatFeatureFlagBit> FORMAT_FEATURE_TABLE = {
        {VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,                     rhi::PixelFormatFeatureFlagBit::COLOR         },
        {VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT,               rhi::PixelFormatFeatureFlagBit::BLEND         },
        {VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,             rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL },
        {VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR, rhi::PixelFormatFeatureFlagBit::SHADING_RATE  },
        {VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,                        rhi::PixelFormatFeatureFlagBit::SAMPLE        },
        {VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,          rhi::PixelFormatFeatureFlagBit::SAMPLE_FILTER },
        {VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,                        rhi::PixelFormatFeatureFlagBit::STORAGE       },
        {VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT,                 rhi::PixelFormatFeatureFlagBit::STORAGE_ATOMIC},
    };

    VkImageType FromRHI(rhi::ImageType type)
    {
        if (type == rhi::ImageType::IMAGE_2D) {
            return VK_IMAGE_TYPE_2D;
        }
        if (type == rhi::ImageType::IMAGE_3D) {
            return VK_IMAGE_TYPE_3D;
        }
        return VK_IMAGE_TYPE_2D;
    }

    VkImageViewType FromRHI(rhi::ImageViewType type)
    {
        if (type == rhi::ImageViewType::VIEW_2D) {
            return VK_IMAGE_VIEW_TYPE_2D;
        }
        if (type == rhi::ImageViewType::VIEW_3D) {
            return VK_IMAGE_VIEW_TYPE_3D;
        }
        if (type == rhi::ImageViewType::VIEW_CUBE) {
            return VK_IMAGE_VIEW_TYPE_CUBE;
        }
        if (type == rhi::ImageViewType::VIEW_2D_ARRAY) {
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        if (type == rhi::ImageViewType::VIEW_CUBE_ARRAY) {
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        }
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    VkFormat FromRHI(rhi::PixelFormat format)
    {
        auto iter = PIXEL_FORMAT_TABLE.find(format);
        return iter == PIXEL_FORMAT_TABLE.end() ? VK_FORMAT_UNDEFINED : iter->second;
    }

    VkFormat FromRHI(rhi::Format format)
    {
        auto iter = FORMAT_TABLE.find(format);
        return iter == FORMAT_TABLE.end() ? VK_FORMAT_UNDEFINED : iter->second;
    }

    VkVertexInputRate FromRHI(rhi::VertexInputRate rate)
    {
        return rate == rhi::VertexInputRate::PER_INSTANCE ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
    }

    VmaMemoryUsage FromRHI(rhi::MemoryType type, const rhi::ImageUsageFlags& usage)
    {
        if (type == rhi::MemoryType::GPU_ONLY) {
            return (usage & rhi::ImageUsageFlagBit::TRANSIENT) ? VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED : VMA_MEMORY_USAGE_GPU_ONLY;
        }
        if (type == rhi::MemoryType::CPU_ONLY) {
            return VMA_MEMORY_USAGE_CPU_ONLY;
        }
        if (type == rhi::MemoryType::CPU_TO_GPU) {
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        }
        if (type == rhi::MemoryType::GPU_TO_CPU) {
            return VMA_MEMORY_USAGE_GPU_TO_CPU;
        }
        return VMA_MEMORY_USAGE_GPU_ONLY;
    }

    VkFilter FromRHI(rhi::Filter filter)
    {
        if (filter == rhi::Filter::LINEAR) {
            return VK_FILTER_LINEAR;
        }
        if (filter == rhi::Filter::NEAREST) {
            return VK_FILTER_NEAREST;
        }
        return VK_FILTER_NEAREST;
    }

    VkSamplerMipmapMode FromRHI(rhi::MipFilter filter)
    {
        if (filter == rhi::MipFilter::LINEAR) {
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
        if (filter == rhi::MipFilter::NEAREST) {
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    VkSamplerAddressMode FromRHI(rhi::WrapMode mode)
    {
        auto iter = SAMPLER_ADDRESS_TABLE.find(mode);
        return iter == SAMPLER_ADDRESS_TABLE.end() ? VK_SAMPLER_ADDRESS_MODE_REPEAT : iter->second;
    }

    VkSampleCountFlagBits FromRHI(rhi::SampleCount sample)
    {
        return static_cast<VkSampleCountFlagBits>(sample);
    }

    VkAttachmentLoadOp FromRHI(rhi::LoadOp op)
    {
        if (op == rhi::LoadOp::LOAD) {
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        }
        if (op == rhi::LoadOp::CLEAR) {
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    VkAttachmentStoreOp FromRHI(rhi::StoreOp op)
    {
        if (op == rhi::StoreOp::STORE) {
            return VK_ATTACHMENT_STORE_OP_STORE;
        }
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    VkCompareOp FromRHI(rhi::CompareOp compare)
    {
        static const std::unordered_map<rhi::CompareOp, VkCompareOp> compareMap =
            {
                {rhi::CompareOp:: NEVER           , VK_COMPARE_OP_NEVER},
                {rhi::CompareOp:: LESS            , VK_COMPARE_OP_LESS},
                {rhi::CompareOp:: EQUAL           , VK_COMPARE_OP_EQUAL},
                {rhi::CompareOp:: LESS_OR_EQUAL   , VK_COMPARE_OP_LESS_OR_EQUAL},
                {rhi::CompareOp:: GREATER         , VK_COMPARE_OP_GREATER},
                {rhi::CompareOp:: NOT_EQUAL       , VK_COMPARE_OP_NOT_EQUAL},
                {rhi::CompareOp:: GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL},
                {rhi::CompareOp:: ALWAYS          , VK_COMPARE_OP_ALWAYS},
            };
        auto iter = compareMap.find(compare);
        return iter == compareMap.end() ? VK_COMPARE_OP_NEVER : iter->second;
    }

    VkPrimitiveTopology FromRHI(rhi::PrimitiveTopology topo)
    {
        static const std::unordered_map<rhi::PrimitiveTopology, VkPrimitiveTopology> topoMap =
            {
                {rhi::PrimitiveTopology::POINT_LIST                   , VK_PRIMITIVE_TOPOLOGY_POINT_LIST},
                {rhi::PrimitiveTopology::LINE_LIST                    , VK_PRIMITIVE_TOPOLOGY_LINE_LIST},
                {rhi::PrimitiveTopology::LINE_STRIP                   , VK_PRIMITIVE_TOPOLOGY_LINE_STRIP},
                {rhi::PrimitiveTopology::TRIANGLE_LIST                , VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
                {rhi::PrimitiveTopology::TRIANGLE_STRIP               , VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP},
                {rhi::PrimitiveTopology::TRIANGLE_FAN                 , VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN},
            };
        auto iter = topoMap.find(topo);
        return iter == topoMap.end() ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : iter->second;
    }

    VkPolygonMode FromRHI(rhi::PolygonMode mode)
    {
        static const std::unordered_map<rhi::PolygonMode, VkPolygonMode> modeMap =
            {
                {rhi::PolygonMode::FILL  , VK_POLYGON_MODE_FILL},
                {rhi::PolygonMode::LINE  , VK_POLYGON_MODE_LINE},
                {rhi::PolygonMode::POINT , VK_POLYGON_MODE_POINT},
            };
        auto iter = modeMap.find(mode);
        return iter == modeMap.end() ? VK_POLYGON_MODE_FILL : iter->second;
    }

    VkFrontFace FromRHI(rhi::FrontFace front)
    {
        return front == rhi::FrontFace::CW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    VkBlendFactor FromRHI(rhi::BlendFactor factor)
    {
        static const std::unordered_map<rhi::BlendFactor, VkBlendFactor> blendFactorMap =
            {
                {rhi::BlendFactor::ZERO                    , VK_BLEND_FACTOR_ZERO                    },
                {rhi::BlendFactor::ONE                     , VK_BLEND_FACTOR_ONE                     },
                {rhi::BlendFactor::SRC_COLOR               , VK_BLEND_FACTOR_SRC_COLOR               },
                {rhi::BlendFactor::ONE_MINUS_SRC_COLOR     , VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR     },
                {rhi::BlendFactor::DST_COLOR               , VK_BLEND_FACTOR_DST_COLOR               },
                {rhi::BlendFactor::ONE_MINUS_DST_COLOR     , VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR     },
                {rhi::BlendFactor::SRC_ALPHA               , VK_BLEND_FACTOR_SRC_ALPHA               },
                {rhi::BlendFactor::ONE_MINUS_SRC_ALPHA     , VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA     },
                {rhi::BlendFactor::DST_ALPHA               , VK_BLEND_FACTOR_DST_ALPHA               },
                {rhi::BlendFactor::ONE_MINUS_DST_ALPHA     , VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA     },
                {rhi::BlendFactor::CONSTANT_COLOR          , VK_BLEND_FACTOR_CONSTANT_COLOR          },
                {rhi::BlendFactor::ONE_MINUS_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR},
                {rhi::BlendFactor::CONSTANT_ALPHA          , VK_BLEND_FACTOR_CONSTANT_ALPHA          },
                {rhi::BlendFactor::ONE_MINUS_CONSTANT_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA},
                {rhi::BlendFactor::SRC_ALPHA_SATURATE      , VK_BLEND_FACTOR_SRC_ALPHA_SATURATE      },
                {rhi::BlendFactor::SRC1_COLOR              , VK_BLEND_FACTOR_SRC1_COLOR              },
                {rhi::BlendFactor::ONE_MINUS_SRC1_COLOR    , VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR    },
                {rhi::BlendFactor::SRC1_ALPHA              , VK_BLEND_FACTOR_SRC1_ALPHA              },
                {rhi::BlendFactor::ONE_MINUS_SRC1_ALPHA    , VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA    },
            };
        auto iter = blendFactorMap.find(factor);
        return iter == blendFactorMap.end() ? VK_BLEND_FACTOR_ZERO : iter->second;
    }

    VkBlendOp FromRHI(rhi::BlendOp op)
    {
        return op == rhi::BlendOp::ADD ? VK_BLEND_OP_ADD : VK_BLEND_OP_SUBTRACT;
    }

    VkIndexType FromRHI(rhi::IndexType type)
    {
        return type == rhi::IndexType::U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    }

    VkStencilOp FromRHI(rhi::StencilOp op)
    {
        static const std::unordered_map<rhi::StencilOp, VkStencilOp> blendFactorMap =
            {
                {rhi::StencilOp::KEEP               , VK_STENCIL_OP_KEEP               },
                {rhi::StencilOp::ZERO               , VK_STENCIL_OP_ZERO               },
                {rhi::StencilOp::REPLACE            , VK_STENCIL_OP_REPLACE            },
                {rhi::StencilOp::INCREMENT_AND_CLAMP, VK_STENCIL_OP_INCREMENT_AND_CLAMP},
                {rhi::StencilOp::DECREMENT_AND_CLAMP, VK_STENCIL_OP_DECREMENT_AND_CLAMP},
                {rhi::StencilOp::INVERT             , VK_STENCIL_OP_INVERT             },
                {rhi::StencilOp::INCREMENT_AND_WRAP , VK_STENCIL_OP_INCREMENT_AND_WRAP },
                {rhi::StencilOp::DECREMENT_AND_WRAP , VK_STENCIL_OP_DECREMENT_AND_WRAP },
            };
        auto iter = blendFactorMap.find(op);
        return iter == blendFactorMap.end() ? VK_STENCIL_OP_KEEP : iter->second;
    }

    VkShaderStageFlags FromRHI(const rhi::ShaderStageFlags& flags)
    {
        VkShaderStageFlags res = {};
        static const std::unordered_map<rhi::ShaderStageFlagBit, VkShaderStageFlagBits> flagMap =
        { {rhi::ShaderStageFlagBit::VS, VK_SHADER_STAGE_VERTEX_BIT},
          {rhi::ShaderStageFlagBit::FS, VK_SHADER_STAGE_FRAGMENT_BIT},
          {rhi::ShaderStageFlagBit::CS, VK_SHADER_STAGE_COMPUTE_BIT},
        };

        for (const auto &[usageBit, vkUsageBit] : flagMap) {
            if (flags & usageBit) {
                res |= vkUsageBit;
            }
        }
        return res;
    }

    VkImageUsageFlags FromRHI(const rhi::ImageUsageFlags& flags)
    {
        VkImageUsageFlags res = {};
        static const std::unordered_map<rhi::ImageUsageFlagBit, VkImageUsageFlagBits> usageMap = {
            {rhi::ImageUsageFlagBit::TRANSFER_SRC    , VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
            {rhi::ImageUsageFlagBit::TRANSFER_DST    , VK_IMAGE_USAGE_TRANSFER_DST_BIT},
            {rhi::ImageUsageFlagBit::SAMPLED         , VK_IMAGE_USAGE_SAMPLED_BIT},
            {rhi::ImageUsageFlagBit::STORAGE         , VK_IMAGE_USAGE_STORAGE_BIT},
            {rhi::ImageUsageFlagBit::RENDER_TARGET   , VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
            {rhi::ImageUsageFlagBit::DEPTH_STENCIL   , VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
            {rhi::ImageUsageFlagBit::TRANSIENT       , VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT},
            {rhi::ImageUsageFlagBit::INPUT_ATTACHMENT, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT},
        };

        for (const auto &[usageBit, vkUsageBit] : usageMap) {
            if (flags & usageBit) {
                res |= vkUsageBit;
            }
        }
        return res;
    }

    VkBufferUsageFlags FromRHI(const rhi::BufferUsageFlags& flags)
    {
        VkBufferUsageFlags res = {};
        static const std::unordered_map<rhi::BufferUsageFlagBit, VkBufferUsageFlagBits> usageMap = {
            {rhi::BufferUsageFlagBit::TRANSFER_SRC, VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
            {rhi::BufferUsageFlagBit::TRANSFER_DST, VK_BUFFER_USAGE_TRANSFER_DST_BIT},
            {rhi::BufferUsageFlagBit::UNIFORM     , VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
            {rhi::BufferUsageFlagBit::STORAGE     , VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
            {rhi::BufferUsageFlagBit::VERTEX      , VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
            {rhi::BufferUsageFlagBit::INDEX       , VK_BUFFER_USAGE_INDEX_BUFFER_BIT},
            {rhi::BufferUsageFlagBit::INDIRECT    , VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT},
        };

        for (const auto &[usageBit, vkUsageBit] : usageMap) {
            if (flags & usageBit) {
                res |= vkUsageBit;
            }
        }
        return res;
    }

    VkImageAspectFlags FromRHI(const rhi::AspectFlags& flags)
    {
        VkImageAspectFlags res = {};
        static const std::unordered_map<rhi::AspectFlagBit, VkImageAspectFlagBits> aspectMap = {
            {rhi::AspectFlagBit::COLOR_BIT  , VK_IMAGE_ASPECT_COLOR_BIT},
            {rhi::AspectFlagBit::DEPTH_BIT  , VK_IMAGE_ASPECT_DEPTH_BIT},
            {rhi::AspectFlagBit::STENCIL_BIT, VK_IMAGE_ASPECT_STENCIL_BIT},
        };

        for (const auto &[bit, vkBit] : aspectMap) {
            if (flags & bit) {
                res |= vkBit;
            }
        }
        return res;
    }

    VkCullModeFlags FromRHI(const rhi::CullingModeFlags& flags)
    {
        VkCullModeFlags ret = VK_CULL_MODE_NONE;
        if (flags & rhi::CullModeFlagBits::FRONT) {
            ret |= VK_CULL_MODE_FRONT_BIT;
        }
        if (flags & rhi::CullModeFlagBits::BACK) {
            ret |= VK_CULL_MODE_BACK_BIT;
        }
        return ret;
    }

    static const std::unordered_map<rhi::PipelineStageBit, VkPipelineStageFlagBits> STAGE_FLAG_MAP = {
        {rhi::PipelineStageBit::TOP            , VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
        {rhi::PipelineStageBit::DRAW_INDIRECT  , VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT},
        {rhi::PipelineStageBit::VERTEX_INPUT   , VK_PIPELINE_STAGE_VERTEX_INPUT_BIT},
        {rhi::PipelineStageBit::VERTEX_SHADER  , VK_PIPELINE_STAGE_VERTEX_SHADER_BIT},
        {rhi::PipelineStageBit::FRAGMENT_SHADER, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
        {rhi::PipelineStageBit::EARLY_FRAGMENT , VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT},
        {rhi::PipelineStageBit::LATE_FRAGMENT  , VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT},
        {rhi::PipelineStageBit::COLOR_OUTPUT   , VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        {rhi::PipelineStageBit::COMPUTE_SHADER , VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
        {rhi::PipelineStageBit::TRANSFER       , VK_PIPELINE_STAGE_TRANSFER_BIT},
        {rhi::PipelineStageBit::BOTTOM         , VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT},
    };
    VkPipelineStageFlags FromRHI(const rhi::PipelineStageFlags& flags)
    {
        VkPipelineStageFlags res = {};
        for (const auto &[bit, vkBit] : STAGE_FLAG_MAP) {
            if (flags & bit) {
                res |= vkBit;
            }
        }
        return res;
    }

    VkPipelineStageFlagBits FromRHI(const rhi::PipelineStageBit& bit)
    {
        return STAGE_FLAG_MAP.at(bit);
    }

    VkStencilOpState FromRHI(const rhi::StencilState& stencil)
    {
        VkStencilOpState ret = {};
        ret.failOp      = FromRHI(stencil.failOp);
        ret.passOp      = FromRHI(stencil.passOp);
        ret.depthFailOp = FromRHI(stencil.depthFailOp);
        ret.compareOp   = FromRHI(stencil.compareOp);
        ret.compareMask = stencil.compareMask;
        ret.writeMask   = stencil.writeMask;
        ret.reference   = stencil.reference;
        return ret;
    }

    VkDescriptorType FromRHI(rhi::DescriptorType type)
    {
        static const std::unordered_map<rhi::DescriptorType, VkDescriptorType> DESCRIPTOR_TYPE_MAP = {
            {rhi::DescriptorType::SAMPLER               , VK_DESCRIPTOR_TYPE_SAMPLER},
            {rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
            {rhi::DescriptorType::SAMPLED_IMAGE         , VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE},
            {rhi::DescriptorType::STORAGE_IMAGE         , VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
            {rhi::DescriptorType::UNIFORM_BUFFER        , VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
            {rhi::DescriptorType::STORAGE_BUFFER        , VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
            {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
            {rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            {rhi::DescriptorType::INPUT_ATTACHMENT      , VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT},
        };
        auto iter = DESCRIPTOR_TYPE_MAP.find(type);
        return iter == DESCRIPTOR_TYPE_MAP.end() ? VK_DESCRIPTOR_TYPE_SAMPLER : iter->second;
    }

    VkImageSubresourceLayers FromRHI(const rhi::ImageSubRangeLayers &res)
    {
        return VkImageSubresourceLayers {
            FromRHI(res.aspectMask),
            res.level,
            res.baseLayer,
            res.layers
        };
    }

    VkQueryType FromRHI(rhi::QueryType type)
    {
        switch (type) {
        case rhi::QueryType::PIPELINE_STATISTICS: return VK_QUERY_TYPE_PIPELINE_STATISTICS;
        case rhi::QueryType::TIME_STAMP: return VK_QUERY_TYPE_TIMESTAMP;
        case rhi::QueryType::OCCLUSION: return VK_QUERY_TYPE_OCCLUSION;
        default: return VK_QUERY_TYPE_MAX_ENUM;
        }
    }

    VkQueryPipelineStatisticFlags FromRHI(const rhi::PipelineStatisticFlags& flags)
    {
        VkQueryPipelineStatisticFlags res = 0;
        if (flags & rhi::PipelineStatisticFlagBits::IA_VERTICES) { res |= VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT;}
        if (flags & rhi::PipelineStatisticFlagBits::IA_PRIMITIVES) { res |= VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT;}
        if (flags & rhi::PipelineStatisticFlagBits::VS_INVOCATIONS) { res |= VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT;}
        if (flags & rhi::PipelineStatisticFlagBits::FS_INVOCATIONS) { res |= VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;}
        if (flags & rhi::PipelineStatisticFlagBits::CLIP_INVOCATIONS) { res |= VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;}
        if (flags & rhi::PipelineStatisticFlagBits::CLIP_PRIMITIVES) { res |= VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT;}
        if (flags & rhi::PipelineStatisticFlagBits::CS_INVOCATIONS) { res |= VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;}
        return res;
    }

    VkDescriptorBindingFlags FromRHI(const rhi::DescriptorBindingFlags &flags)
    {
        VkDescriptorBindingFlags res = 0;
        if (flags & rhi::DescriptorBindingFlagBit::VARIABLE_COUNT) { res |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;}
        return res;
    }

    rhi::PixelFormatFeatureFlags ToRHI(VkFormatFeatureFlags flags)
    {
        rhi::PixelFormatFeatureFlags res;
        for (const auto &[vk, rhi] : FORMAT_FEATURE_TABLE) {
            if (flags & vk) {
                res |= rhi;
            }
        }
        return res;
    }

} // namespace sky::vk
