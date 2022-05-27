//
// Created by Zach Lee on 2022/5/27.
//


#include <engine/render/resources/Buffer.h>
#include <engine/render/DriverManager.h>

namespace sky {

    Buffer::Buffer(const Descriptor& desc) : descriptor(desc)
    {
        if (desc.useHost) {
            rawData.resize(desc.size);
        }
    }

    void Buffer::InitRHI()
    {
        drv::Buffer::Descriptor desc = {};
        desc.size = descriptor.size;
        desc.memory = descriptor.memory;
        desc.usage = descriptor.usage;
        rhiBuffer = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::Buffer>(desc);
    }

    bool Buffer::IsValid() const
    {
        return !!rhiBuffer;
    }

}