//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Buffer.h>
#include <mtl/Device.h>

namespace sky::mtl {

    Buffer::Buffer(Device &dev) : DevObject(dev)
    {
    }

    bool Buffer::Init(const Descriptor &desc)
    {
        bufferDesc = desc;
        auto *mtlDevice = device.GetMetalDevice();

        MTL::ResourceOptions options = {};
        buffer = mtlDevice->newBuffer(desc.size, options);
    }

}
