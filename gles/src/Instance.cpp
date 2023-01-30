//
// Created by Zach on 2023/1/30.
//

#include <gles/Instance.h>
#include <gles/Device.h>
#include <core/platform/Platform.h>

namespace sky::gles {

    rhi::Device *Instance::CreateDevice(const rhi::Device::Descriptor &desc)
    {
        auto device = std::make_unique<Device>();
        if (device->Init(desc)) {
            return device.release();
        }
        return nullptr;
    }

}

extern "C" SKY_EXPORT sky::rhi::Instance *CreateInstance()
{
    return new sky::gles::Instance();
}
