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
        ~Shader();

        struct VkDescriptor {
            VkShaderStageFlagBits stage;
            const uint32_t       *spv  = nullptr;
            uint32_t              size = 0;
        };

        VkShaderModule GetNativeHandle() const;

        VkShaderStageFlagBits GetShaderStage() const;

        uint32_t GetHash() const;

    private:
        friend class Device;
        Shader(Device &);

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        VkShaderModule        shaderModule;
        VkShaderStageFlagBits stage;
        uint32_t              hash;
    };

    using ShaderPtr = std::shared_ptr<Shader>;

} // namespace sky::vk
