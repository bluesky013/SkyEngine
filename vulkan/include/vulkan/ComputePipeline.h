//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/PipelineLayout.h"
#include "vulkan/Shader.h"
#include "vulkan/ShaderOption.h"
#include "vulkan/vulkan.h"
#include <string>
#include <vector>
#include <array>

namespace sky::vk {

    class Device;

    class ComputePipeline : public DevObject {
    public:
        ~ComputePipeline();

        struct Descriptor {
            ShaderPtr         shader;
            ShaderOptionPtr   shaderOption;
            PipelineLayoutPtr pipelineLayout;
        };

        bool Init(const Descriptor &);

        VkPipeline GetNativeHandle() const;
    private:
        friend class Device;
        ComputePipeline(Device &);

        static uint32_t CalculateHash(const Descriptor &);

        VkPipeline        pipeline;
        uint32_t          hash;
        PipelineLayoutPtr pipelineLayout;
    };

    using ComputePipelinePtr = std::shared_ptr<ComputePipeline>;

} // namespace sky::vk
