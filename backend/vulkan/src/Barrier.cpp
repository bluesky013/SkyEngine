//
// Created by Zach Lee on 2023/5/13.
//

#include <vulkan/Barrier.h>

#include <string>

#include <core/platform/Platform.h>

namespace sky::vk {

    static constexpr VkPipelineStageFlags COLOR_IN_OUT_STAGE_BITS = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    static constexpr VkPipelineStageFlags DEPTH_IN_OUT_STAGE_BITS = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    static constexpr VkPipelineStageFlags DEPTH_OUT_STAGE_BITS = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

    std::vector<AccessInfo> ACCESS_TABLE = {
        {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT                         , VK_ACCESS_NONE                                                                    , VK_IMAGE_LAYOUT_UNDEFINED                                   },
        {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT                       , VK_ACCESS_INDIRECT_COMMAND_READ_BIT                                               , VK_IMAGE_LAYOUT_UNDEFINED                                   },
        {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT                        , VK_ACCESS_INDEX_READ_BIT                                                          , VK_IMAGE_LAYOUT_UNDEFINED                                   },
        {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT                        , VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT                                               , VK_IMAGE_LAYOUT_UNDEFINED                                   },
        {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                       , VK_ACCESS_UNIFORM_READ_BIT                                                        , VK_IMAGE_LAYOUT_UNDEFINED                                   },
        {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                       , VK_ACCESS_SHADER_READ_BIT                                                         , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                    },
        {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                       , VK_ACCESS_SHADER_READ_BIT                                                         , VK_IMAGE_LAYOUT_GENERAL                                     },
        {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                       , VK_ACCESS_SHADER_WRITE_BIT                                                        , VK_IMAGE_LAYOUT_GENERAL                                     },
        {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                     , VK_ACCESS_UNIFORM_READ_BIT                                                        , VK_IMAGE_LAYOUT_UNDEFINED                                   },
        {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                     , VK_ACCESS_SHADER_READ_BIT                                                         , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                    },
        {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                     , VK_ACCESS_SHADER_READ_BIT                                                         , VK_IMAGE_LAYOUT_GENERAL                                     },
        {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                     , VK_ACCESS_SHADER_WRITE_BIT                                                        , VK_IMAGE_LAYOUT_GENERAL                                     },
        {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT                      , VK_ACCESS_UNIFORM_READ_BIT                                                        , VK_IMAGE_LAYOUT_UNDEFINED                                   },
        {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT                      , VK_ACCESS_SHADER_READ_BIT                                                         , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                    },
        {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT                      , VK_ACCESS_SHADER_READ_BIT                                                         , VK_IMAGE_LAYOUT_GENERAL                                     },
        {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT                      , VK_ACCESS_SHADER_WRITE_BIT                                                        , VK_IMAGE_LAYOUT_GENERAL                                     },
        {VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR, VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR                           , VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR},
        {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                     , VK_ACCESS_INPUT_ATTACHMENT_READ_BIT                                               , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                    },
        {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                     , VK_ACCESS_INPUT_ATTACHMENT_READ_BIT                                               , VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL             },
        {COLOR_IN_OUT_STAGE_BITS                                   , VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT         , VK_IMAGE_LAYOUT_GENERAL                                     }, // VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
        {COLOR_IN_OUT_STAGE_BITS                                   , VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT        , VK_IMAGE_LAYOUT_GENERAL                                     }, // VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
        {DEPTH_IN_OUT_STAGE_BITS                                   , VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT , VK_IMAGE_LAYOUT_GENERAL                                     }, // VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
        {DEPTH_IN_OUT_STAGE_BITS                                   , VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL                                     }, // VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
        {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT             , VK_ACCESS_COLOR_ATTACHMENT_READ_BIT                                               , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL                    },
        {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT             , VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT                                              , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL                    },
        {DEPTH_OUT_STAGE_BITS                                      , VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT                                       , VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL            },
        {DEPTH_OUT_STAGE_BITS                                      , VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT                                      , VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL            },
        {VK_PIPELINE_STAGE_TRANSFER_BIT                            , VK_ACCESS_TRANSFER_READ_BIT                                                       , VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL                        },
        {VK_PIPELINE_STAGE_TRANSFER_BIT                            , VK_ACCESS_TRANSFER_WRITE_BIT                                                      , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL                        },
        {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                      , VK_ACCESS_NONE                                                                    , VK_IMAGE_LAYOUT_PRESENT_SRC_KHR                             },
        {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT                        , VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT                            , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL                        },
    };

    void ValidateAccessInfoMapByExtension(const std::vector<VkExtensionProperties>& extensions)
    {
//        if (CheckExtension(extensions, "VK_EXT_attachment_feedback_loop_layout")) {
//            ACCESS_TABLE[static_cast<uint32_t>(rhi::AccessFlag::COLOR_INOUT_READ)].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT;
//            ACCESS_TABLE[static_cast<uint32_t>(rhi::AccessFlag::COLOR_INOUT_WRITE)].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT;
//            ACCESS_TABLE[static_cast<uint32_t>(rhi::AccessFlag::DEPTH_STENCIL_INOUT_READ)].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT;
//            ACCESS_TABLE[static_cast<uint32_t>(rhi::AccessFlag::DEPTH_STENCIL_INOUT_WRITE)].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT;
//        }
    }

    AccessInfo GetAccessInfo(const std::vector<rhi::AccessFlag> &accesses, bool ignoreLayout)
    {
        AccessInfo accessInfo = {};
        for (auto &access : accesses) {
            auto index = static_cast<uint32_t>(access);
            SKY_ASSERT(index < ACCESS_TABLE.size());
            auto &info = ACCESS_TABLE[index];

            SKY_ASSERT(ignoreLayout || accessInfo.imageLayout == VK_IMAGE_LAYOUT_UNDEFINED || accessInfo.imageLayout == info.imageLayout);
            accessInfo.pipelineStages |= info.pipelineStages;
            accessInfo.accessFlags |= info.accessFlags;
            accessInfo.imageLayout = info.imageLayout;
        }
        return accessInfo;
    }
} // namespace sky::vk