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

    ComputePipeline::~ComputePipeline()
    {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device.GetNativeHandle(), pipeline, VKL_ALLOC);
        }
    }

    bool ComputePipeline::Init(const Descriptor &desc)
    {
        pipelineLayout = std::static_pointer_cast<PipelineLayout>(desc.pipelineLayout);

        VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout->GetNativeHandle();

        auto shader = std::static_pointer_cast<Shader>(desc.shader);
        pipelineInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage  = shader->GetShaderStage();
        pipelineInfo.stage.module = shader->GetNativeHandle();
        pipelineInfo.stage.pName  = "main";
        pipelineInfo.stage.pSpecializationInfo = nullptr;

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

} // namespace sky::vk
