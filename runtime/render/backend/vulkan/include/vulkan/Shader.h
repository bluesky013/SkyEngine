//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "rhi/Shader.h"
#include <vector>

namespace sky::vk {

    class Device;

    class Shader : public rhi::Shader, public DevObject {
    public:
        ~Shader() override;

        VkShaderModule GetNativeHandle() const;
        VkShaderStageFlagBits GetShaderStage() const;

    private:
        friend class Device;
        explicit Shader(Device &);

        bool Init(const Descriptor &);

        VkShaderModule        shaderModule;
        VkShaderStageFlagBits vkStage;
    };

    using ShaderPtr = std::shared_ptr<Shader>;

} // namespace sky::vk
