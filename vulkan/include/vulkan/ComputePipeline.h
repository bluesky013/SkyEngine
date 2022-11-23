//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include <rhi/ComputePipeline.h>
#include <vulkan/DevObject.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/Shader.h>
#include <vulkan/ShaderOption.h>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <array>

namespace sky::vk {

    class Device;

    class ComputePipeline : public rhi::ComputePipeline, public DevObject {
    public:
        ~ComputePipeline();

        struct VkDescriptor {
            ShaderPtr         shader;
            ShaderOptionPtr   shaderOption;
            PipelineLayoutPtr pipelineLayout;
        };

        VkPipeline GetNativeHandle() const;
    private:
        friend class Device;
        ComputePipeline(Device &);

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        static uint32_t CalculateHash(const VkDescriptor &);

        VkPipeline        pipeline;
        uint32_t          hash;
        PipelineLayoutPtr pipelineLayout;
    };

    using ComputePipelinePtr = std::shared_ptr<ComputePipeline>;

} // namespace sky::vk
