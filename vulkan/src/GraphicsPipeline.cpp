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

    bool GraphicsPipeline::Init(const Descriptor& des)
    {
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        /* Shader */
        pipelineInfo.stageCount;
        pipelineInfo.pStages;

        pipelineInfo.pVertexInputState;

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = des.state.inputAssembly.topology;
        inputAssemblyInfo.primitiveRestartEnable = des.state.inputAssembly.primitiveRestartEnable;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;

        pipelineInfo.pTessellationState = nullptr;

        VkPipelineViewportStateCreateInfo vpState = {};
        vpState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpState.viewportCount = 1;
        vpState.scissorCount = 1;
        pipelineInfo.pViewportState = &vpState;

        VkPipelineRasterizationStateCreateInfo rasterState = {};
        rasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterState.depthClampEnable = des.state.raster.depthClampEnable;
        rasterState.rasterizerDiscardEnable = des.state.raster.rasterizerDiscardEnable;
        rasterState.polygonMode = des.state.raster.polygonMode;
        rasterState.cullMode = des.state.raster.cullMode;
        rasterState.frontFace = des.state.raster.frontFace;
        rasterState.depthBiasEnable = des.state.raster.depthBiasEnable;
        rasterState.depthBiasConstantFactor = des.state.raster.depthBiasConstantFactor;
        rasterState.depthBiasClamp = des.state.raster.depthBiasClamp;
        rasterState.depthBiasSlopeFactor = des.state.raster.depthBiasSlopeFactor;
        rasterState.lineWidth = des.state.raster.lineWidth;
        pipelineInfo.pRasterizationState = &rasterState;

        VkPipelineMultisampleStateCreateInfo multiSampleInfo = {};
        multiSampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multiSampleInfo.rasterizationSamples = des.state.multiSample.rasterizationSamples;
        multiSampleInfo.sampleShadingEnable = des.state.multiSample.sampleShadingEnable;
        multiSampleInfo.minSampleShading = des.state.multiSample.minSampleShading;
        multiSampleInfo.pSampleMask = nullptr;
        multiSampleInfo.alphaToCoverageEnable = des.state.multiSample.alphaToCoverageEnable;
        multiSampleInfo.alphaToOneEnable = des.state.multiSample.alphaToOneEnable;
        pipelineInfo.pMultisampleState = &multiSampleInfo;

        VkPipelineDepthStencilStateCreateInfo dsInfo = {};
        dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsInfo.depthTestEnable = des.state.depthStencil.depthTestEnable;
        dsInfo.depthWriteEnable = des.state.depthStencil.depthWriteEnable;
        dsInfo.depthCompareOp = des.state.depthStencil.depthCompareOp;
        dsInfo.depthBoundsTestEnable = des.state.depthStencil.depthBoundsTestEnable;
        dsInfo.stencilTestEnable = des.state.depthStencil.stencilTestEnable;
        dsInfo.front = des.state.depthStencil.front;
        dsInfo.back = des.state.depthStencil.back;
        dsInfo.minDepthBounds = des.state.depthStencil.minDepthBounds;
        dsInfo.maxDepthBounds = des.state.depthStencil.maxDepthBounds;
        pipelineInfo.pDepthStencilState = &dsInfo;

        VkPipelineColorBlendStateCreateInfo blendInfo = {};
        blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blendInfo.logicOpEnable = VK_FALSE;
        blendInfo.logicOp = VK_LOGIC_OP_CLEAR;
        blendInfo.attachmentCount = static_cast<uint32_t>(des.state.blends.attachments.size());
        blendInfo.pAttachments = des.state.blends.attachments.data();
        blendInfo.blendConstants[0] = 0;
        blendInfo.blendConstants[1] = 0;
        blendInfo.blendConstants[2] = 0;
        blendInfo.blendConstants[3] = 0;
        pipelineInfo.pColorBlendState = &blendInfo;

        VkPipelineDynamicStateCreateInfo dynStates = {};
        dynStates.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        VkDynamicState dynStatesData[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        dynStates.dynamicStateCount = sizeof(dynStatesData) / sizeof(VkDynamicState);
        dynStates.pDynamicStates = dynStatesData;
        pipelineInfo.pDynamicState = &dynStates;

        pipelineInfo.layout = device.GetPipelineLayout(des.pipelineLayout);
        pipelineInfo.renderPass = device.GetRenderPass(des.renderPass);
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