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
            std::vector<uint32_t> spv;
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