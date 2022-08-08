//
// Created by Zach Lee on 2022/1/9.
//

#pragma once

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "vulkan/Shader.h"
#include "vulkan/ShaderOption.h"
#include "vulkan/PipelineLayout.h"
#include "vulkan/RenderPass.h"
#include "vulkan/VertexInput.h"
#include <vector>
#include <string>

namespace sky::drv {

    class Device;

    class GraphicsPipeline : public DevObject {
    public:
        ~GraphicsPipeline() = default;

        struct InputAssembly {
            VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkBool32            primitiveRestartEnable = VK_FALSE;
        };

        struct Raster {
            VkBool32        depthClampEnable        = VK_FALSE;
            VkBool32        rasterizerDiscardEnable = VK_FALSE;
            VkPolygonMode   polygonMode             = VK_POLYGON_MODE_FILL;
            VkCullModeFlags cullMode                = VK_CULL_MODE_NONE;
            VkFrontFace     frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            VkBool32        depthBiasEnable         = VK_FALSE;
            float           depthBiasConstantFactor = 0.f;
            float           depthBiasClamp          = 0.f;
            float           depthBiasSlopeFactor    = 0.f;
            float           lineWidth               = 1.f;
        };

        struct MultiSample {
            VkSampleCountFlagBits samples               = VK_SAMPLE_COUNT_1_BIT;
            VkBool32              sampleShadingEnable   = VK_FALSE;
            float                 minSampleShading      = 0.f;
            VkBool32              alphaToCoverageEnable = VK_FALSE;
            VkBool32              alphaToOneEnable      = VK_FALSE;
        };

        struct DepthStencilState {
            VkBool32         depthTestEnable  = false;
            VkBool32         depthWriteEnable = false;
            VkCompareOp      depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
            VkBool32         depthBoundsTestEnable = VK_FALSE;
            VkBool32         stencilTestEnable     = VK_FALSE;
            VkStencilOpState front = {};
            VkStencilOpState back  = {};
            float            minDepthBounds = 0.f;
            float            maxDepthBounds = 1.f;
        };

        struct BlendState {
            VkBool32              blendEnable = VK_FALSE;
            VkBlendFactor         srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            VkBlendFactor         dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            VkBlendOp             colorBlendOp = VK_BLEND_OP_ADD;
            VkBlendFactor         srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            VkBlendFactor         dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            VkBlendOp             alphaBlendOp = VK_BLEND_OP_ADD;
            VkColorComponentFlags colorWriteMask = 0xF;
        };

        struct ColorBlend {
            uint32_t attachmentNum = 1;
            BlendState attachments[4];
        };

        struct State {
            InputAssembly inputAssembly;
            Raster raster;
            MultiSample multiSample;
            ColorBlend blends;
            DepthStencilState depthStencil;
        };

        struct Program {
            std::vector<ShaderPtr> shaders;
            ShaderOptionPtr shaderOption;
        };

        struct Descriptor {
            const State* state = nullptr;
            const Program* program = nullptr;
            VertexInputPtr vertexInput;
            RenderPassPtr renderPass;
            PipelineLayoutPtr pipelineLayout;
            uint32_t subPassIndex = 0;
        };

        bool Init(const Descriptor&);

        VkPipeline GetNativeHandle() const;

        PipelineLayoutPtr GetPipelineLayout() const;

    private:
        friend class Device;
        GraphicsPipeline(Device&);

        static uint32_t CalculateHash(const Descriptor&);

        VkPipeline pipeline;
        uint32_t hash;
        RenderPassPtr renderPass;
        PipelineLayoutPtr pipelineLayout;
    };

    using GraphicsPipelinePtr = std::shared_ptr<GraphicsPipeline>;

}