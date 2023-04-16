//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Instance.h>
#include <mtl/Device.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>
#include <core/util/DynamicModule.h>

static const char* TAG = "Metal";

namespace sky::mtl {

    static void PrintDevice(MTL::Device *dev)
    {
        LOG_I(TAG, "/*********Device Info Begin*********/");
        LOG_I(TAG, "Device name: %s", dev->name()->utf8String());
        LOG_I(TAG, "Device registryID: %llu", dev->registryID());
        LOG_I(TAG, "Device location: %u", dev->location());
        LOG_I(TAG, "Device locationNumber: %u", dev->locationNumber());
        LOG_I(TAG, "Device lowPower: %d", dev->lowPower());
        LOG_I(TAG, "Device removable: %d", dev->removable());
        LOG_I(TAG, "Device headless: %d", dev->headless());
        LOG_I(TAG, "/**********Device Info End**********/");
    }

    Instance::~Instance() noexcept
    {
    }

    rhi::Device *Instance::CreateDevice(const rhi::Device::Descriptor &des)
    {
        auto *device = new Device(*this);
        if (!device->Init(des)) {
            delete device;
            device = nullptr;
        }
        return device;
    }

    bool Instance::Init(const Descriptor &des)
    {
        auto *array = MTL::CopyAllDevices();
        uint32_t size = array->count();
        for (uint32_t i = 0; i < size; ++i) {
            auto *mtlDevice = static_cast<MTL::Device*>(array->object(i));
            PrintDevice(mtlDevice);
            devices.emplace_back(mtlDevice);
        }

        array->release();
        return true;
    }

    const std::vector<MTL::Device*> &Instance::GetMtlDevices() const
    {
        return devices;
    }
}

extern "C" SKY_EXPORT sky::rhi::Instance *CreateInstance()
{
    return new sky::mtl::Instance();
}

