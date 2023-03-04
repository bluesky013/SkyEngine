//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <vulkan/Conversion.h>
#include <vulkan/Device.h>
#include <vulkan/GraphicsPipeline.h>

namespace sky::vk {

    uint32_t GraphicsPipeline::CalculateHash(const Descriptor &desc)
    {
        uint32_t hash = 0;
        HashCombine32(hash, Crc32::Cal(desc.state.depthStencil));
        HashCombine32(hash, Crc32::Cal(desc.state.multiSample));
        HashCombine32(hash, Crc32::Cal(desc.state.inputAssembly));
        HashCombine32(hash, Crc32::Cal(desc.state.rasterState));
        HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t*>(desc.state.blendStates.data()),
            static_cast<uint32_t>(desc.state.blendStates.size() * sizeof(rhi::BlendState))));

        HashCombine32(hash, std::static_pointer_cast<Shader>(desc.vs)->GetHash());
        HashCombine32(hash, std::static_pointer_cast<Shader>(desc.fs)->GetHash());
        HashCombine32(hash, std::static_pointer_cast<RenderPass>(desc.renderPass)->GetHash());
        HashCombine32(hash, std::static_pointer_cast<PipelineLayout>(desc.pipelineLayout)->GetHash());
        HashCombine32(hash, std::static_pointer_cast<VertexInput>(desc.vertexInput)->GetHash());
        HashCombine32(hash, desc.subPassIndex);
        return hash;
    }

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

    static void FillShaderStageInfo(std::vector<VkPipelineShaderStageCreateInfo> &shaderStageInfo, const rhi::ShaderPtr &shader)
    {
        if (!shader) {
            return;
        }
        auto vkShader = std::static_pointer_cast<Shader>(shader);
        VkPipelineShaderStageCreateInfo stageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};

        stageInfo.stage  = vkShader->GetShaderStage();
        stageInfo.module = vkShader->GetNativeHandle();
        stageInfo.pName  = "main";
        shaderStageInfo.emplace_back(stageInfo);
    }

    bool GraphicsPipeline::Init(const Descriptor &des)
    {
        VkGraphicsPipelineCreateInfo pipelineInfo = {};

        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // shaders
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfo;
        FillShaderStageInfo(shaderStageInfo, des.vs);
        FillShaderStageInfo(shaderStageInfo, des.fs);
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
        pipelineInfo.pStages    = shaderStageInfo.data();

        // vertex input
        pipelineInfo.pVertexInputState = std::static_pointer_cast<VertexInput>(des.vertexInput)->GetInfo();

        // input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssemblyInfo.topology               = FromRHI(des.state.inputAssembly.topology);
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;

        // viewport
        VkPipelineViewportStateCreateInfo vpState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vpState.viewportCount        = 1;
        vpState.scissorCount         = 1;
        pipelineInfo.pViewportState  = &vpState;

        // raster
        VkPipelineRasterizationStateCreateInfo rasterState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterState.depthClampEnable        = des.state.rasterState.depthClampEnable;
        rasterState.rasterizerDiscardEnable = des.state.rasterState.rasterizerDiscardEnable;
        rasterState.polygonMode             = FromRHI(des.state.rasterState.polygonMode);
        rasterState.cullMode                = FromRHI(des.state.rasterState.cullMode);
        rasterState.frontFace               = FromRHI(des.state.rasterState.frontFace);
        rasterState.depthBiasEnable         = des.state.rasterState.depthBiasEnable;
        rasterState.depthBiasConstantFactor = des.state.rasterState.depthBiasConstantFactor;
        rasterState.depthBiasClamp          = des.state.rasterState.depthBiasClamp;
        rasterState.depthBiasSlopeFactor    = des.state.rasterState.depthBiasSlopeFactor;
        rasterState.lineWidth               = des.state.rasterState.lineWidth;
        pipelineInfo.pRasterizationState    = &rasterState;

        // multi sample
        VkPipelineMultisampleStateCreateInfo multiSampleInfo = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multiSampleInfo.rasterizationSamples  = FromRHI(des.state.multiSample.sampleCount);
        multiSampleInfo.sampleShadingEnable   = VK_FALSE;
        multiSampleInfo.minSampleShading      = 0.f;
        multiSampleInfo.pSampleMask           = nullptr;
        multiSampleInfo.alphaToCoverageEnable = VK_FALSE;
        multiSampleInfo.alphaToOneEnable      = VK_FALSE;
        pipelineInfo.pMultisampleState = &multiSampleInfo;

        // depth stencil
        VkPipelineDepthStencilStateCreateInfo dsInfo = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dsInfo.depthTestEnable          = des.state.depthStencil.depthTest;
        dsInfo.depthWriteEnable         = des.state.depthStencil.depthWrite;
        dsInfo.depthCompareOp           = FromRHI(des.state.depthStencil.compareOp);
        dsInfo.depthBoundsTestEnable    = VK_FALSE;
        dsInfo.stencilTestEnable        = des.state.depthStencil.stencilTest;
        dsInfo.front                    = FromRHI(des.state.depthStencil.front);
        dsInfo.back                     = FromRHI(des.state.depthStencil.back);
        dsInfo.minDepthBounds           = des.state.depthStencil.minDepth;
        dsInfo.maxDepthBounds           = des.state.depthStencil.maxDepth;
        pipelineInfo.pDepthStencilState = &dsInfo;

        // color blend
        std::vector<VkPipelineColorBlendAttachmentState> colorAttachments(des.state.blendStates.size());
        for (uint32_t i = 0; i < des.state.blendStates.size(); ++i) {
            auto &attachment = colorAttachments[i];
            auto &inColor = des.state.blendStates[i];
            attachment.blendEnable = inColor.blendEn;
            attachment.srcColorBlendFactor = FromRHI(inColor.srcColor);
            attachment.dstColorBlendFactor = FromRHI(inColor.dstColor);
            attachment.colorBlendOp = FromRHI(inColor.colorBlendOp);
            attachment.srcAlphaBlendFactor = FromRHI(inColor.srcAlpha);
            attachment.dstAlphaBlendFactor = FromRHI(inColor.dstAlpha);
            attachment.alphaBlendOp = FromRHI(inColor.alphaBlendOp);
            attachment.colorWriteMask = inColor.writeMask;
        }
        VkPipelineColorBlendStateCreateInfo blendInfo = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blendInfo.logicOpEnable   = VK_FALSE;
        blendInfo.logicOp         = VK_LOGIC_OP_CLEAR;
        blendInfo.attachmentCount = static_cast<uint32_t>(colorAttachments.size());
        blendInfo.pAttachments    = colorAttachments.data();
        pipelineInfo.pColorBlendState = &blendInfo;

        // dynamic
        VkPipelineDynamicStateCreateInfo dynStates = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        VkDynamicState dynStatesData[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        dynStates.dynamicStateCount    = sizeof(dynStatesData) / sizeof(VkDynamicState);
        dynStates.pDynamicStates       = dynStatesData;
        pipelineInfo.pDynamicState     = &dynStates;

        pipelineLayout = std::static_pointer_cast<PipelineLayout>(des.pipelineLayout);
        renderPass     = std::static_pointer_cast<RenderPass>(des.renderPass);

        pipelineInfo.layout             = pipelineLayout->GetNativeHandle();
        pipelineInfo.renderPass         = renderPass->GetNativeHandle();
        pipelineInfo.subpass            = des.subPassIndex;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex  = 0;

        auto tmpHash = CalculateHash(des);
        pipeline     = device.GetPipeline(tmpHash, &pipelineInfo);
        if (pipeline == VK_NULL_HANDLE) {
            return false;
        }
        hash = tmpHash;
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

        VkPipelineFragmentShadingRateStateCreateInfoKHR fragmentShadingRateState = {};
        if (device.GetFeatures().variableRateShading) {
            fragmentShadingRateState.sType = VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR;
            fragmentShadingRateState.pNext = nullptr;
            fragmentShadingRateState.fragmentSize   = des.state->vrs.fragmentSize;
            // Combiner for pipeline (A) and primitive (B)
            fragmentShadingRateState.combinerOps[0] = des.state->vrs.combinerOps[0];
            // Combiner for pipeline (A) and attachment (B)
            fragmentShadingRateState.combinerOps[1] = des.state->vrs.combinerOps[1];
            pipelineInfo.pNext = &fragmentShadingRateState;
        }

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
