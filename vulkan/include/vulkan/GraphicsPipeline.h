//
// Created by Zach Lee on 2022/1/9.
//

#pragma once

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include <vector>

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
            VkSampleCountFlagBits rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
            VkBool32              sampleShadingEnable   = VK_FALSE;
            float                 minSampleShading      = 0.f;
            VkBool32              alphaToCoverageEnable = VK_FALSE;
            VkBool32              alphaToOneEnable      = VK_FALSE;
        };

        struct DepthStencilState {
            VkBool32         depthTestEnable = false;
            VkBool32         depthWriteEnable = false;
            VkCompareOp      depthCompareOp = VK_COMPARE_OP_NEVER;
            VkBool32         depthBoundsTestEnable = VK_FALSE;
            VkBool32         stencilTestEnable = VK_FALSE;
            VkStencilOpState front = {};
            VkStencilOpState back  = {};
            float            minDepthBounds = 0.f;
            float            maxDepthBounds = 1.f;
        };

        struct ColorBlend {
            std::vector<VkPipelineColorBlendAttachmentState> attachments;
        };

        struct PipelineState {
            InputAssembly inputAssembly;
            Raster raster;
            MultiSample multiSample;
            ColorBlend blends;
            DepthStencilState depthStencil;
        };

        struct Descriptor {
            PipelineState state;
            uint32_t pipelineLayout = 0;
            uint32_t renderPass = 0;
        };

        bool Init(const Descriptor&);

        VkPipeline GetNativeHandle() const;

    private:
        friend class Device;
        GraphicsPipeline(Device&);

        VkPipeline pipeline;
        uint32_t hash;
    };

    using GraphicsPipelinePtr = std::shared_ptr<GraphicsPipeline>;

}