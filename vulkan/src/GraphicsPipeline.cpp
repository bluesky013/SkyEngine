//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <vulkan/Device.h>
#include <vulkan/GraphicsPipeline.h>

namespace sky::vk {

    uint32_t GraphicsPipeline::CalculateHash(const VkDescriptor &desc)
    {
        uint32_t hash = 0;
        HashCombine32(hash, Crc32::Cal(*desc.state));
        for (auto &shader : desc.program->shaders) {
            HashCombine32(hash, shader->GetHash());
        }
        if (desc.program->shaderOption) {
            HashCombine32(hash, desc.program->shaderOption->GetHash());
        }
        HashCombine32(hash, desc.pipelineLayout->GetHash());
        HashCombine32(hash, desc.renderPass->GetHash());
        HashCombine32(hash, desc.vertexInput->GetHash());
        HashCombine32(hash, desc.subPassIndex);
        return hash;
    }

    GraphicsPipeline::GraphicsPipeline(Device &dev) : DevObject(dev), pipeline(VK_NULL_HANDLE), hash(0)
    {
    }

    bool GraphicsPipeline::Init(const Descriptor &des)
    {
        return true;
    }

    bool GraphicsPipeline::Init(const VkDescriptor &des)
    {
        if (des.state == nullptr || des.program == nullptr || !des.vertexInput || !des.pipelineLayout || !des.renderPass) {
            return false;
        }
        auto &pipelineState = *des.state;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        /* Shader */
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfo;
        for (auto &shader : des.program->shaders) {
            VkPipelineShaderStageCreateInfo stageInfo = {};
            stageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage                           = shader->GetShaderStage();
            stageInfo.module                          = shader->GetNativeHandle();
            stageInfo.pName                           = "main";

            if (des.program->shaderOption) {
                stageInfo.pSpecializationInfo = des.program->shaderOption->GetSpecializationInfo(stageInfo.stage);
            }
            shaderStageInfo.emplace_back(std::move(stageInfo));
        }
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
        pipelineInfo.pStages    = shaderStageInfo.data();

        pipelineInfo.pVertexInputState = des.vertexInput->GetInfo();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
        inputAssemblyInfo.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology                               = pipelineState.inputAssembly.topology;
        inputAssemblyInfo.primitiveRestartEnable                 = pipelineState.inputAssembly.primitiveRestartEnable;
        pipelineInfo.pInputAssemblyState                         = &inputAssemblyInfo;

        pipelineInfo.pTessellationState = nullptr;

        VkPipelineViewportStateCreateInfo vpState = {};
        vpState.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpState.viewportCount                     = 1;
        vpState.scissorCount                      = 1;
        pipelineInfo.pViewportState               = &vpState;

        VkPipelineRasterizationStateCreateInfo rasterState = {};
        rasterState.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterState.depthClampEnable                       = pipelineState.raster.depthClampEnable;
        rasterState.rasterizerDiscardEnable                = pipelineState.raster.rasterizerDiscardEnable;
        rasterState.polygonMode                            = pipelineState.raster.polygonMode;
        rasterState.cullMode                               = pipelineState.raster.cullMode;
        rasterState.frontFace                              = pipelineState.raster.frontFace;
        rasterState.depthBiasEnable                        = pipelineState.raster.depthBiasEnable;
        rasterState.depthBiasConstantFactor                = pipelineState.raster.depthBiasConstantFactor;
        rasterState.depthBiasClamp                         = pipelineState.raster.depthBiasClamp;
        rasterState.depthBiasSlopeFactor                   = pipelineState.raster.depthBiasSlopeFactor;
        rasterState.lineWidth                              = pipelineState.raster.lineWidth;
        pipelineInfo.pRasterizationState                   = &rasterState;

        VkPipelineMultisampleStateCreateInfo multiSampleInfo = {};
        multiSampleInfo.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multiSampleInfo.rasterizationSamples                 = pipelineState.multiSample.samples;
        multiSampleInfo.sampleShadingEnable                  = pipelineState.multiSample.sampleShadingEnable;
        multiSampleInfo.minSampleShading                     = pipelineState.multiSample.minSampleShading;
        multiSampleInfo.pSampleMask                          = nullptr;
        multiSampleInfo.alphaToCoverageEnable                = pipelineState.multiSample.alphaToCoverageEnable;
        multiSampleInfo.alphaToOneEnable                     = pipelineState.multiSample.alphaToOneEnable;
        pipelineInfo.pMultisampleState                       = &multiSampleInfo;

        VkPipelineDepthStencilStateCreateInfo dsInfo = {};
        dsInfo.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsInfo.depthTestEnable                       = pipelineState.depthStencil.depthTestEnable;
        dsInfo.depthWriteEnable                      = pipelineState.depthStencil.depthWriteEnable;
        dsInfo.depthCompareOp                        = pipelineState.depthStencil.depthCompareOp;
        dsInfo.depthBoundsTestEnable                 = pipelineState.depthStencil.depthBoundsTestEnable;
        dsInfo.stencilTestEnable                     = pipelineState.depthStencil.stencilTestEnable;
        dsInfo.front                                 = pipelineState.depthStencil.front;
        dsInfo.back                                  = pipelineState.depthStencil.back;
        dsInfo.minDepthBounds                        = pipelineState.depthStencil.minDepthBounds;
        dsInfo.maxDepthBounds                        = pipelineState.depthStencil.maxDepthBounds;
        pipelineInfo.pDepthStencilState              = &dsInfo;

        VkPipelineColorBlendStateCreateInfo blendInfo = {};
        blendInfo.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blendInfo.logicOpEnable                       = VK_FALSE;
        blendInfo.logicOp                             = VK_LOGIC_OP_CLEAR;
        blendInfo.attachmentCount                     = static_cast<uint32_t>(pipelineState.blends.blendStates.size());
        blendInfo.pAttachments                        = (VkPipelineColorBlendAttachmentState *)(pipelineState.blends.blendStates.data());
        blendInfo.blendConstants[0]                   = 0;
        blendInfo.blendConstants[1]                   = 0;
        blendInfo.blendConstants[2]                   = 0;
        blendInfo.blendConstants[3]                   = 0;
        pipelineInfo.pColorBlendState                 = &blendInfo;

        VkPipelineDynamicStateCreateInfo dynStates = {};
        dynStates.sType                            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        VkDynamicState dynStatesData[]             = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        dynStates.dynamicStateCount                = sizeof(dynStatesData) / sizeof(VkDynamicState);
        dynStates.pDynamicStates                   = dynStatesData;
        pipelineInfo.pDynamicState                 = &dynStates;

        pipelineInfo.layout             = des.pipelineLayout->GetNativeHandle();
        pipelineInfo.renderPass         = des.renderPass->GetNativeHandle();
        pipelineInfo.subpass            = des.subPassIndex;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex  = 0;

        auto tmpHash = CalculateHash(des);
        pipeline     = device.GetPipeline(tmpHash, &pipelineInfo);
        if (pipeline == VK_NULL_HANDLE) {
            return false;
        }
        hash = tmpHash;

        renderPass     = des.renderPass;
        pipelineLayout = des.pipelineLayout;
        return true;
    }

    VkPipeline GraphicsPipeline::GetNativeHandle() const
    {
        return pipeline;
    }

    PipelineLayoutPtr GraphicsPipeline::GetPipelineLayout() const
    {
        return pipelineLayout;
    }
} // namespace sky::vk
