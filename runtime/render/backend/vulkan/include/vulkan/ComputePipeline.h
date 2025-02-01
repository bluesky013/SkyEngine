//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include <rhi/ComputePipeline.h>
#include <vulkan/DevObject.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/Shader.h>
#include <vulkan/ShaderConstants.h>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <array>

namespace sky::vk {

    class Device;

    class ComputePipeline : public rhi::ComputePipeline, public DevObject {
    public:
        ~ComputePipeline();

        VkPipeline GetNativeHandle() const;
    private:
        friend class Device;
        ComputePipeline(Device &);

        bool Init(const Descriptor &);

        VkPipeline        pipeline;
        uint32_t          hash;
        PipelineLayoutPtr pipelineLayout;
    };

    using ComputePipelinePtr = std::shared_ptr<ComputePipeline>;

} // namespace sky::vk
