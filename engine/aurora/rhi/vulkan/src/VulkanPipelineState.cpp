//
// Created by Zach Lee on 2026/4/1.
//

#include "VulkanPipelineState.h"
#include "VulkanConversion.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include <core/logger/Logger.h>

namespace sky::aurora {

    static const char *TAG = "VulkanPipelineState";

    // ---- VulkanGraphicsPipeline ----

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        if (pipeline != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyPipeline(device.GetNativeHandle(), pipeline, nullptr);
        }
    }

    bool VulkanGraphicsPipeline::Init(const Descriptor &desc)
    {
        const auto &fmt = desc.format;

        // -- dynamic rendering path via VkPipelineRenderingCreateInfo --
        VkFormat colorFormats[MAX_COLOR_ATTACHMENTS] = {};
        for (uint32_t i = 0; i < fmt.numColors; ++i) {
            colorFormats[i] = FromPixelFormat(fmt.colors[i]);
        }

        const VkFormat dsFormat = FromPixelFormat(fmt.depthStencil);
        const bool hasStencil =
            fmt.depthStencil == PixelFormat::D24_S8 ||
            fmt.depthStencil == PixelFormat::D32_S8;

        VkPipelineRenderingCreateInfo renderingInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        renderingInfo.viewMask                = fmt.viewMask;
        renderingInfo.colorAttachmentCount    = fmt.numColors;
        renderingInfo.pColorAttachmentFormats = colorFormats;
        renderingInfo.depthAttachmentFormat   = dsFormat;
        renderingInfo.stencilAttachmentFormat = hasStencil ? dsFormat : VK_FORMAT_UNDEFINED;

        return BuildPipeline(desc, &renderingInfo);
    }

    bool VulkanGraphicsPipeline::Init(const Descriptor &desc, const SubpassInfo &subpassInfo)
    {
        // Legacy path: renderPass + subpass are set directly on the pipeline
        // create info inside BuildPipeline via the pNext chain placeholder.
        // We pass nullptr for pNext and patch renderPass/subpass after build.

        // Build a temporary VkPipelineRenderingCreateInfo that carries the
        // SubpassInfo data.  BuildPipeline will detect the renderPass and use
        // the legacy code path.
        return BuildPipeline(desc, &subpassInfo);
    }

    bool VulkanGraphicsPipeline::BuildPipeline(const Descriptor &desc, const void *pNext)
    {
        if (desc.state == nullptr) {
            LOG_E(TAG, "graphics pipeline descriptor missing state");
            return false;
        }

        const auto &state = *desc.state;
        const auto &fmt   = desc.format;

        // ---- shader stages ----
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        if (desc.shader != nullptr) {
            const auto *vkShader = static_cast<const VulkanShader *>(desc.shader);
            if (const auto *vs = vkShader->GetVertexFunction(); vs != nullptr) {
                VkPipelineShaderStageCreateInfo stageCI = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
                stageCI.stage  = FromShaderStage(vs->GetStage());
                stageCI.module = vs->GetNativeHandle();
                stageCI.pName  = "main";
                shaderStages.push_back(stageCI);
            }
            if (const auto *fs = vkShader->GetFragmentFunction(); fs != nullptr) {
                VkPipelineShaderStageCreateInfo stageCI = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
                stageCI.stage  = FromShaderStage(fs->GetStage());
                stageCI.module = fs->GetNativeHandle();
                stageCI.pName  = "main";
                shaderStages.push_back(stageCI);
            }
        }

        // ---- vertex input (empty - vertex pulling / mesh shaders) ----
        VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

        // ---- input assembly ----
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology               = FromPrimitiveTopology(state.inputAssembly.topology);
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // ---- viewport (dynamic) ----
        VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        // ---- rasterizer ----
        VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.depthClampEnable        = static_cast<VkBool32>(state.rasterState.depthClampEnable);
        rasterizer.rasterizerDiscardEnable = static_cast<VkBool32>(state.rasterState.rasterizerDiscardEnable);
        rasterizer.polygonMode             = FromPolygonMode(state.rasterState.polygonMode);
        rasterizer.cullMode                = FromCullMode(state.rasterState.cullMode);
        rasterizer.frontFace               = FromFrontFace(state.rasterState.frontFace);
        rasterizer.depthBiasEnable         = static_cast<VkBool32>(state.rasterState.depthBiasEnable);
        rasterizer.depthBiasConstantFactor = state.rasterState.depthBiasConstantFactor;
        rasterizer.depthBiasClamp          = state.rasterState.depthBiasClamp;
        rasterizer.depthBiasSlopeFactor    = state.rasterState.depthBiasSlopeFactor;
        rasterizer.lineWidth               = state.rasterState.lineWidth;

        // ---- multisample ----
        VkPipelineMultisampleStateCreateInfo multisample = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples  = FromSampleCount(fmt.sampleCount);
        multisample.sampleShadingEnable   = VK_FALSE;
        multisample.minSampleShading      = 0.f;
        multisample.pSampleMask           = nullptr;
        multisample.alphaToCoverageEnable = static_cast<VkBool32>(state.multiSample.alphaToCoverage);
        multisample.alphaToOneEnable      = VK_FALSE;

        // ---- depth stencil ----
        VkPipelineDepthStencilStateCreateInfo depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable       = static_cast<VkBool32>(state.depthStencil.depthTest);
        depthStencil.depthWriteEnable      = static_cast<VkBool32>(state.depthStencil.depthWrite);
        depthStencil.depthCompareOp        = FromCompareOp(state.depthStencil.compareOp);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable     = static_cast<VkBool32>(state.depthStencil.stencilTest);
        depthStencil.front                 = FromStencilState(state.depthStencil.front);
        depthStencil.back                  = FromStencilState(state.depthStencil.back);
        depthStencil.minDepthBounds        = state.depthStencil.minDepth;
        depthStencil.maxDepthBounds        = state.depthStencil.maxDepth;

        // ---- color blend ----
        std::vector<VkPipelineColorBlendAttachmentState> colorAttachments(fmt.numColors);
        for (uint32_t i = 0; i < fmt.numColors; ++i) {
            auto &att = colorAttachments[i];
            if (i < state.blendStates.size()) {
                const auto &src  = state.blendStates[i];
                att.blendEnable         = static_cast<VkBool32>(src.blendEn);
                att.srcColorBlendFactor = FromBlendFactor(src.srcColor);
                att.dstColorBlendFactor = FromBlendFactor(src.dstColor);
                att.colorBlendOp        = FromBlendOp(src.colorBlendOp);
                att.srcAlphaBlendFactor = FromBlendFactor(src.srcAlpha);
                att.dstAlphaBlendFactor = FromBlendFactor(src.dstAlpha);
                att.alphaBlendOp        = FromBlendOp(src.alphaBlendOp);
                att.colorWriteMask      = src.writeMask;
            } else {
                att.blendEnable    = VK_FALSE;
                att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            }
        }

        VkPipelineColorBlendStateCreateInfo colorBlend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlend.logicOpEnable   = VK_FALSE;
        colorBlend.logicOp         = VK_LOGIC_OP_CLEAR;
        colorBlend.attachmentCount = static_cast<uint32_t>(colorAttachments.size());
        colorBlend.pAttachments    = colorAttachments.data();

        // ---- dynamic state ----
        const VkDynamicState dynStatesData[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynState.dynamicStateCount = static_cast<uint32_t>(sizeof(dynStatesData) / sizeof(dynStatesData[0]));
        dynState.pDynamicStates    = dynStatesData;

        // ---- assemble create info ----
        VkGraphicsPipelineCreateInfo ci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        ci.stageCount          = static_cast<uint32_t>(shaderStages.size());
        ci.pStages             = shaderStages.data();
        ci.pVertexInputState   = &vertexInput;
        ci.pInputAssemblyState = &inputAssembly;
        ci.pViewportState      = &viewportState;
        ci.pRasterizationState = &rasterizer;
        ci.pMultisampleState   = &multisample;
        ci.pDepthStencilState  = &depthStencil;
        ci.pColorBlendState    = &colorBlend;
        ci.pDynamicState       = &dynState;
        ci.layout              = VK_NULL_HANDLE; // TODO: set from shader pipeline layout
        ci.basePipelineHandle  = VK_NULL_HANDLE;
        ci.basePipelineIndex   = -1;

        // Determine path: dynamic rendering vs legacy render pass
        const auto *subpass = static_cast<const SubpassInfo *>(pNext);
        if (subpass != nullptr && subpass->renderPass != VK_NULL_HANDLE) {
            // Legacy render pass path
            ci.renderPass = subpass->renderPass;
            ci.subpass    = subpass->subpass;
        } else {
            // Dynamic rendering path - pNext points to VkPipelineRenderingCreateInfo
            ci.renderPass = VK_NULL_HANDLE;
            ci.pNext      = pNext;
        }

        const VkResult result = device.GetDeviceFn().vkCreateGraphicsPipelines(
            device.GetNativeHandle(), VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create graphics pipeline, VkResult=%d", static_cast<int>(result));
            return false;
        }

        return true;
    }

    // ---- VulkanComputePipeline ----

    VulkanComputePipeline::VulkanComputePipeline(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanComputePipeline::~VulkanComputePipeline()
    {
        if (pipeline != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyPipeline(device.GetNativeHandle(), pipeline, nullptr);
        }
    }

    bool VulkanComputePipeline::Init(const Descriptor &desc, VkPipelineLayout layout)
    {
        VkComputePipelineCreateInfo ci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.layout = layout;

        ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        if (desc.cs != nullptr) {
            const auto *vkShader = static_cast<const VulkanShader *>(desc.cs);
            if (const auto *cs = vkShader->GetComputeFunction(); cs != nullptr) {
                ci.stage.module = cs->GetNativeHandle();
                ci.stage.pName  = "main";
            }
        }

        const VkResult result = device.GetDeviceFn().vkCreateComputePipelines(
            device.GetNativeHandle(), VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create compute pipeline, VkResult=%d", static_cast<int>(result));
            return false;
        }

        return true;
    }

} // namespace sky::aurora