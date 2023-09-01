//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <vector>
#include <memory>
#include <rhi/Device.h>

namespace sky {

    class Shader {
    public:
        Shader() = default;
        ~Shader() = default;

        bool Init(rhi::ShaderStageFlagBit stage, const uint8_t *data, uint32_t size);

    private:
        std::vector<uint32_t> data; // spv, glsl or msl
        rhi::Shader::Descriptor shaderDesc;
        rhi::ShaderPtr shader;
    };
    using RDShaderPtr = std::shared_ptr<Shader>;

    class Program {
    public:
        Program() = default;
        ~Program() = default;

    private:
        std::vector<rhi::ShaderPtr> shaders;
    };
    using RDProgramPtr = std::shared_ptr<Program>;

} // namespace sky
