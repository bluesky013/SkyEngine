//
// Created by Zach Lee on 2022/1/9.
//

#include <vulkan/ComputePipeline.h>
#include <vulkan/Device.h>
#include <core/logger/Logger.h>

static const char* TAG = "Vulkan";

namespace sky::vk {
    ComputePipeline::ComputePipeline(Device &dev) : DevObject(dev), pipeline(VK_NULL_HANDLE), hash(0)
    {

    }

    bool ComputePipeline::Init(const Descriptor &desc)
    {
        VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = desc.pipelineLayout->GetNativeHandle();

        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = desc.shader->GetShaderStage();
        pipelineInfo.stage.module = desc.shader->GetNativeHandle();
        pipelineInfo.stage.pName = "main";
        pipelineInfo.stage.pSpecializationInfo = desc.shaderOption ? desc.shaderOption->GetSpecializationInfo(VK_SHADER_STAGE_FRAGMENT_BIT) : nullptr;

        pipelineLayout = desc.pipelineLayout;

        auto rst = vkCreateComputePipelines(device.GetNativeHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, VKL_ALLOC, &pipeline);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create Pipeline failed, %d", rst);
        }
        return pipeline;
    }

    VkPipeline ComputePipeline::GetNativeHandle() const
    {
        return pipeline;
    }

    uint32_t ComputePipeline::CalculateHash(const Descriptor &desc)
    {
        return 0;
    }

} // namespace sky::vk
