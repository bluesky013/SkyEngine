//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/Shader.h>
#include <dx12/Device.h>

namespace sky::dx {

    Shader::Shader(Device &dev) : DevObject(dev)
    {
    }

    Shader::~Shader()
    {
    }

    bool Shader::Init(const Descriptor &)
    {
        return true;
    }
}