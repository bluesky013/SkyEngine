//
// Created by Zach Lee on 2022/1/9.
//

#include <vulkan/GraphicsPipeline.h>
#include <vulkan/Device.h>
#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>

namespace sky::drv {

    uint32_t GraphicsPipeline::CalculateHash(const Descriptor& desc)
    {
        return 0;
    }

    GraphicsPipeline::GraphicsPipeline(Device& dev) : DevObject(dev), pipeline(VK_NULL_HANDLE), hash(0)
    {

    }

    bool GraphicsPipeline::Init(const Descriptor& des)
    {
        if (des.state == nullptr) {
            return false;
        }
        auto& pipelineState = *des.state;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        /* Shader */
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfo;
        for (auto& shaderInfo : des.program->shaders) {
            VkPipelineShaderStageCreateInfo stageInfo = {};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = shaderInfo.shader->GetShaderStage();
            stageInfo.module = shaderInfo.shader->GetNativeHandle();
            stageInfo.pName = shaderInfo.entry.c_str();

            if (des.program->shaderOption) {
                stageInfo.pSpecializationInfo = des.program->shaderOption->GetSpecializationInfo(stageInfo.stage);
            }
            shaderStageInfo.emplace_back(std::move(stageInfo));
        }
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
        pipelineInfo.pStages = shaderStageInfo.data();

        pipelineInfo.pVertexInputState = des.vertexInput->GetInfo();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = pipelineState.inputAssembly.topology;
        inputAssemblyInfo.primitiveRestartEnable = pipelineState.inputAssembly.primitiveRestartEnable;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;

        pipelineInfo.pTessellationState = nullptr;

        VkPipelineViewportStateCreateInfo vpState = {};
        vpState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpState.viewportCount = 1;
        vpState.scissorCount = 1;
        pipelineInfo.pViewportState = &vpState;

        VkPipelineRasterizationStateCreateInfo rasterState = {};
        rasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterState.depthClampEnable        = pipelineState.raster.depthClampEnable;
        rasterState.rasterizerDiscardEnable = pipelineState.raster.rasterizerDiscardEnable;
        rasterState.polygonMode             = pipelineState.raster.polygonMode;
        rasterState.cullMode                = pipelineState.raster.cullMode;
        rasterState.frontFace               = pipelineState.raster.frontFace;
        rasterState.depthBiasEnable         = pipelineState.raster.depthBiasEnable;
        rasterState.depthBiasConstantFactor = pipelineState.raster.depthBiasConstantFactor;
        rasterState.depthBiasClamp          = pipelineState.raster.depthBiasClamp;
        rasterState.depthBiasSlopeFactor    = pipelineState.raster.depthBiasSlopeFactor;
        rasterState.lineWidth               = pipelineState.raster.lineWidth;
        pipelineInfo.pRasterizationState    = &rasterState;

        VkPipelineMultisampleStateCreateInfo multiSampleInfo = {};
        multiSampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multiSampleInfo.rasterizationSamples  = pipelineState.multiSample.samples;
        multiSampleInfo.sampleShadingEnable   = pipelineState.multiSample.sampleShadingEnable;
        multiSampleInfo.minSampleShading      = pipelineState.multiSample.minSampleShading;
        multiSampleInfo.pSampleMask           = nullptr;
        multiSampleInfo.alphaToCoverageEnable = pipelineState.multiSample.alphaToCoverageEnable;
        multiSampleInfo.alphaToOneEnable      = pipelineState.multiSample.alphaToOneEnable;
        pipelineInfo.pMultisampleState        = &multiSampleInfo;

        VkPipelineDepthStencilStateCreateInfo dsInfo = {};
        dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsInfo.depthTestEnable       = pipelineState.depthStencil.depthTestEnable;
        dsInfo.depthWriteEnable      = pipelineState.depthStencil.depthWriteEnable;
        dsInfo.depthCompareOp        = pipelineState.depthStencil.depthCompareOp;
        dsInfo.depthBoundsTestEnable = pipelineState.depthStencil.depthBoundsTestEnable;
        dsInfo.stencilTestEnable     = pipelineState.depthStencil.stencilTestEnable;
        dsInfo.front                 = pipelineState.depthStencil.front;
        dsInfo.back                  = pipelineState.depthStencil.back;
        dsInfo.minDepthBounds        = pipelineState.depthStencil.minDepthBounds;
        dsInfo.maxDepthBounds        = pipelineState.depthStencil.maxDepthBounds;
        pipelineInfo.pDepthStencilState = &dsInfo;

        VkPipelineColorBlendStateCreateInfo blendInfo = {};
        blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blendInfo.logicOpEnable = VK_FALSE;
        blendInfo.logicOp = VK_LOGIC_OP_CLEAR;
        blendInfo.attachmentCount = pipelineState.blends.attachmentNum;
        blendInfo.pAttachments = (VkPipelineColorBlendAttachmentState*)(pipelineState.blends.attachments);
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

        pipelineInfo.layout = des.pipelineLayout->GetNativeHandle();
        pipelineInfo.renderPass = des.renderPass->GetNativeHandle();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = 0;

        auto rst = vkCreateGraphicsPipelines(device.GetNativeHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, VKL_ALLOC, &pipeline);
        if (rst != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    VkPipeline GraphicsPipeline::GetNativeHandle() const
    {
        return pipeline;
    }
}