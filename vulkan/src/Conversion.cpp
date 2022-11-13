//
// Created by Zach Lee on 2022/11/10.
//

#include <vulkan/Conversion.h>
#include <unordered_map>

namespace sky::vk {

    std::unordered_map<rhi::PixelFormat, VkFormat> FORMAT_TABLE = {
        {rhi::PixelFormat::UNDEFINED,                 VK_FORMAT_UNDEFINED,               },
        {rhi::PixelFormat::RGBA8_UNORM,               VK_FORMAT_R8G8B8A8_UNORM,          },
        {rhi::PixelFormat::RGBA8_SRGB,                VK_FORMAT_R8G8B8A8_SRGB,           },
        {rhi::PixelFormat::BGRA8_UNORM,               VK_FORMAT_B8G8R8A8_UNORM,          },
        {rhi::PixelFormat::BGRA8_SRGB,                VK_FORMAT_B8G8R8A8_SRGB,           },
        {rhi::PixelFormat::D32,                       VK_FORMAT_D32_SFLOAT,              },
        {rhi::PixelFormat::D24_S8,                    VK_FORMAT_D24_UNORM_S8_UINT,       },
        {rhi::PixelFormat::D32_S8,                    VK_FORMAT_D32_SFLOAT_S8_UINT,      },
        {rhi::PixelFormat::BC1_RGB_UNORM_BLOCK,       VK_FORMAT_BC1_RGB_UNORM_BLOCK,     },
        {rhi::PixelFormat::BC1_RGB_SRGB_BLOCK,        VK_FORMAT_BC1_RGB_SRGB_BLOCK,      },
        {rhi::PixelFormat::BC1_RGBA_UNORM_BLOCK,      VK_FORMAT_BC1_RGBA_UNORM_BLOCK,    },
        {rhi::PixelFormat::BC1_RGBA_SRGB_BLOCK,       VK_FORMAT_BC1_RGBA_SRGB_BLOCK,     },
        {rhi::PixelFormat::BC2_UNORM_BLOCK,           VK_FORMAT_BC2_UNORM_BLOCK,         },
        {rhi::PixelFormat::BC2_SRGB_BLOCK,            VK_FORMAT_BC2_SRGB_BLOCK,          },
        {rhi::PixelFormat::BC3_UNORM_BLOCK,           VK_FORMAT_BC3_UNORM_BLOCK,         },
        {rhi::PixelFormat::BC3_SRGB_BLOCK,            VK_FORMAT_BC3_SRGB_BLOCK,          },
        {rhi::PixelFormat::BC4_UNORM_BLOCK,           VK_FORMAT_BC4_UNORM_BLOCK,         },
        {rhi::PixelFormat::BC4_SNORM_BLOCK,           VK_FORMAT_BC4_SNORM_BLOCK,         },
        {rhi::PixelFormat::BC5_UNORM_BLOCK,           VK_FORMAT_BC5_UNORM_BLOCK,         },
        {rhi::PixelFormat::BC5_SNORM_BLOCK,           VK_FORMAT_BC5_SNORM_BLOCK,         },
        {rhi::PixelFormat::BC6H_UFLOAT_BLOCK,         VK_FORMAT_BC6H_UFLOAT_BLOCK,       },
        {rhi::PixelFormat::BC6H_SFLOAT_BLOCK,         VK_FORMAT_BC6H_SFLOAT_BLOCK,       },
        {rhi::PixelFormat::BC7_UNORM_BLOCK,           VK_FORMAT_BC7_UNORM_BLOCK,         },
        {rhi::PixelFormat::BC7_SRGB_BLOCK,            VK_FORMAT_BC7_SRGB_BLOCK,          },
        {rhi::PixelFormat::ETC2_R8G8B8_UNORM_BLOCK,   VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, },
        {rhi::PixelFormat::ETC2_R8G8B8_SRGB_BLOCK,    VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,  },
        {rhi::PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK},
        {rhi::PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK,  VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,},
        {rhi::PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK},
        {rhi::PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK,  VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,},
        {rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK,      VK_FORMAT_ASTC_4x4_UNORM_BLOCK,    },
        {rhi::PixelFormat::ASTC_4x4_SRGB_BLOCK,       VK_FORMAT_ASTC_4x4_SRGB_BLOCK,     },
        {rhi::PixelFormat::ASTC_8x8_UNORM_BLOCK,      VK_FORMAT_ASTC_8x8_UNORM_BLOCK,    },
        {rhi::PixelFormat::ASTC_8x8_SRGB_BLOCK,       VK_FORMAT_ASTC_8x8_SRGB_BLOCK,     },
        {rhi::PixelFormat::ASTC_10x10_UNORM_BLOCK,    VK_FORMAT_ASTC_10x10_UNORM_BLOCK,  },
        {rhi::PixelFormat::ASTC_10x10_SRGB_BLOCK,     VK_FORMAT_ASTC_10x10_SRGB_BLOCK,   },
        {rhi::PixelFormat::ASTC_12x12_UNORM_BLOCK,    VK_FORMAT_ASTC_12x12_UNORM_BLOCK,  },
        {rhi::PixelFormat::ASTC_12x12_SRGB_BLOCK,     VK_FORMAT_ASTC_12x12_SRGB_BLOCK,   },
    };

    std::unordered_map<rhi::WrapMode, VkSamplerAddressMode> SAMPLER_ADDRESS_TABLE = {
            {rhi::WrapMode::REPEAT,               VK_SAMPLER_ADDRESS_MODE_REPEAT},
            {rhi::WrapMode::MIRRORED_REPEAT,      VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT},
            {rhi::WrapMode::CLAMP_TO_EDGE,        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            {rhi::WrapMode::CLAMP_TO_BORDER,      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER},
            {rhi::WrapMode::MIRROR_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE},
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
        return VK_IMAGE_VIEW_TYPE_2D;
    }

    VkFormat FromRHI(rhi::PixelFormat format)
    {
        auto iter = FORMAT_TABLE.find(format);
        return iter == FORMAT_TABLE.end() ? VK_FORMAT_UNDEFINED : iter->second;
    }

    VmaMemoryUsage FromRHI(rhi::MemoryType type)
    {
        if (type == rhi::MemoryType::GPU_ONLY) {
            return VMA_MEMORY_USAGE_GPU_ONLY;
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

    VkShaderStageFlags FromRHI(rhi::ShaderStageFlags flags)
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

    VkImageUsageFlags FromRHI(rhi::ImageUsageFlags flags)
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

    VkBufferUsageFlags FromRHI(rhi::BufferUsageFlags flags)
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

    VkImageAspectFlags FromRHI(rhi::AspectFlags flags)
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


}