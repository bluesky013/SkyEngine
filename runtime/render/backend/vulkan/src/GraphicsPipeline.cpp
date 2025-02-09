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
        HashCombine32(hash, Crc32::Cal(desc.state.multiSample.alphaToCoverage));
        HashCombine32(hash, Crc32::Cal(desc.state.multiSample.sampleCount));
        HashCombine32(hash, Crc32::Cal(desc.state.inputAssembly));
        HashCombine32(hash, Crc32::Cal(desc.state.rasterState));
        HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t*>(desc.state.blendStates.data()),
            static_cast<uint32_t>(desc.state.blendStates.size() * sizeof(rhi::BlendState))));

        if (desc.tas) {
            HashCombine32(hash, std::static_pointer_cast<Shader>(desc.tas)->GetHash());
        }

        if (desc.vs) {
            HashCombine32(hash, std::static_pointer_cast<Shader>(desc.vs)->GetHash());
        }

        if (desc.ms) {
            HashCombine32(hash, std::static_pointer_cast<Shader>(desc.ms)->GetHash());
        }

        HashCombine32(hash, std::static_pointer_cast<Shader>(desc.fs)->GetHash());
        HashCombine32(hash, std::static_pointer_cast<RenderPass>(desc.renderPass)->GetHash());
        HashCombine32(hash, std::static_pointer_cast<PipelineLayout>(desc.pipelineLayout)->GetHash());
        HashCombine32(hash, std::static_pointer_cast<VertexInput>(desc.vertexInput)->GetHash());
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
        stageInfo.pName  = shader->GetEntry().c_str();
        shaderStageInfo.emplace_back(stageInfo);
    }

    bool GraphicsPipeline::Init(const Descriptor &des)
    {
        VkGraphicsPipelineCreateInfo pipelineInfo = {};

        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // shaders
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfo;

        bool meshShading = !des.vs && des.ms;

        FillShaderStageInfo(shaderStageInfo, des.vs);
        FillShaderStageInfo(shaderStageInfo, des.fs);
        FillShaderStageInfo(shaderStageInfo, des.tas);
        FillShaderStageInfo(shaderStageInfo, des.ms);


        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
        pipelineInfo.pStages    = shaderStageInfo.data();

        // vertex input
        inputDesc = std::static_pointer_cast<VertexInput>(des.vertexInput);
        if (!meshShading) {
            pipelineInfo.pVertexInputState = inputDesc->GetInfo();
        }

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
        rasterState.depthClampEnable        = static_cast<VkBool32>(des.state.rasterState.depthClampEnable);
        rasterState.rasterizerDiscardEnable = static_cast<VkBool32>(des.state.rasterState.rasterizerDiscardEnable);
        rasterState.polygonMode             = FromRHI(des.state.rasterState.polygonMode);
        rasterState.cullMode                = FromRHI(des.state.rasterState.cullMode);
        rasterState.frontFace               = FromRHI(des.state.rasterState.frontFace);
        rasterState.depthBiasEnable         = static_cast<VkBool32>(des.state.rasterState.depthBiasEnable);
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
        multiSampleInfo.alphaToCoverageEnable = static_cast<VkBool32>(des.state.multiSample.alphaToCoverage);
        multiSampleInfo.alphaToOneEnable      = VK_FALSE;
        pipelineInfo.pMultisampleState = &multiSampleInfo;

        // depth stencil
        VkPipelineDepthStencilStateCreateInfo dsInfo = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dsInfo.depthTestEnable          = static_cast<VkBool32>(des.state.depthStencil.depthTest);
        dsInfo.depthWriteEnable         = static_cast<VkBool32>(des.state.depthStencil.depthWrite);
        dsInfo.depthCompareOp           = FromRHI(des.state.depthStencil.compareOp);
        dsInfo.depthBoundsTestEnable    = VK_FALSE;
        dsInfo.stencilTestEnable        = static_cast<VkBool32>(des.state.depthStencil.stencilTest);
        dsInfo.front                    = FromRHI(des.state.depthStencil.front);
        dsInfo.back                     = FromRHI(des.state.depthStencil.back);
        dsInfo.minDepthBounds           = des.state.depthStencil.minDepth;
        dsInfo.maxDepthBounds           = des.state.depthStencil.maxDepth;
        pipelineInfo.pDepthStencilState = &dsInfo;

        // color blend
        std::vector<VkPipelineColorBlendAttachmentState> colorAttachments(des.state.blendStates.size());
        for (uint32_t i = 0; i < des.state.blendStates.size(); ++i) {
            auto &attachment = colorAttachments[i];
            const auto &inColor = des.state.blendStates[i];
            attachment.blendEnable = static_cast<VkBool32>(inColor.blendEn);
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

        auto num = pipelineLayout->GetSetNumber();
        for (uint32_t i = 0; i < num; ++i) {
            if (pipelineLayout->GetLayout(i)->GetDescriptorNum() != 0) {
                descriptorMask |= 1 << (i);
            }
        }

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

    VkPipeline GraphicsPipeline::GetNativeHandle() const
    {
        return pipeline;
    }

    PipelineLayoutPtr GraphicsPipeline::GetPipelineLayout() const
    {
        return pipelineLayout;
    }
} // namespace sky::vk
