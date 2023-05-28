//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Instance.h>
#include <string>

#include <core/logger/Logger.h>
#include <core/platform/Platform.h>
#include <core/util/DynamicModule.h>

#include <mtl/Device.h>

static const char* TAG = "Metal";

namespace sky::mtl {

    static void PrintDevice(id<MTLDevice> dev)
    {
        LOG_I(TAG, "/*********Device Info Begin*********/");
        LOG_I(TAG, "Device name: %s", std::string([dev.name UTF8String]).c_str());
        LOG_I(TAG, "Device registryID: %llu", dev.registryID);
        LOG_I(TAG, "Device location: %u", dev.location);
        LOG_I(TAG, "Device locationNumber: %u", dev.locationNumber);
        LOG_I(TAG, "Device lowPower: %d", dev.lowPower);
        LOG_I(TAG, "Device removable: %d", dev.removable);
        LOG_I(TAG, "Device headless: %d", dev.headless);
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
        NSArray<id<MTLDevice>> *array = MTLCopyAllDevices();
        for (id device in array) {
            PrintDevice(device);
            devices.emplace_back(device);
        }

        [array release];
        return true;
    }
}

extern "C" SKY_EXPORT sky::rhi::Instance *CreateInstance()
{
    return new sky::mtl::Instance();
}

