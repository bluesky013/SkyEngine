//
// Created by Zach Lee on 2022/1/9.
//

#include <vulkan/GraphicsPipeline.h>
#include <vulkan/Device.h>
#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>

namespace sky::drv {

    GraphicsPipeline::GraphicsPipeline(Device& dev) : DevObject(dev), pipeline(VK_NULL_HANDLE), hash(0)
    {

    }

    bool GraphicsPipeline::Init(const Descriptor&)
    {
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount;
        pipelineInfo.pStages;
        pipelineInfo.pVertexInputState;
        pipelineInfo.pInputAssemblyState;
        pipelineInfo.pTessellationState;
        pipelineInfo.pViewportState;
        pipelineInfo.pRasterizationState;
        pipelineInfo.pMultisampleState;
        pipelineInfo.pDepthStencilState;
        pipelineInfo.pColorBlendState;
        pipelineInfo.pDynamicState;
        pipelineInfo.layout;
        pipelineInfo.renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = 0;
        return true;
    }

    VkPipeline GraphicsPipeline::GetNativeHandle() const
    {
        return pipeline;
    }
}