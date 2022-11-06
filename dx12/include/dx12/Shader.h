//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class Shader : public DevObject {
    public:
        Shader(Device &);
        ~Shader();

        struct Descriptor {
        };

    private:
        friend class Device;
        bool Init(const Descriptor &);

        D3D12_SHADER_BYTECODE byteCode;
    };

}