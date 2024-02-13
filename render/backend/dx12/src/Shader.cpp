//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/Shader.h>
#include <dx12/Device.h>

namespace sky::dx {

    Shader::Shader(Device &dev) : DevObject(dev)
    {
    }

    bool Shader::Init(const Descriptor &desc)
    {
        stage = desc.stage;

        storage.resize(desc.size);
        memcpy(storage.data(), desc.data, desc.size);

        byteCode.BytecodeLength = desc.size;
        byteCode.pShaderBytecode = storage.data();
        return true;
    }
}