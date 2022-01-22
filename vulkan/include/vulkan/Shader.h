//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include <vector>

namespace sky::drv {

    class Device;

    class Shader : public DevObject {
    public:
        ~Shader();

        struct Descriptor {
            VkShaderStageFlagBits stage;
            uint32_t* spv = nullptr;
            uint32_t size = 0;
        };

        bool Init(const Descriptor&);

        VkShaderModule GetNativeHandle() const;

    private:
        friend class Device;
        Shader(Device&);

        VkShaderModule shaderModule;
        uint32_t hash;
    };

    using ShaderPtr = std::shared_ptr<Shader>;

}