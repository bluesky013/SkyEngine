//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <rhi/Shader.h>
#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class Shader : public rhi::Shader, public DevObject {
    public:
        explicit Shader(Device &);
        ~Shader() override = default;

    private:
        friend class Device;
        bool Init(const Descriptor &);

        D3D12_SHADER_BYTECODE byteCode;
    };

}