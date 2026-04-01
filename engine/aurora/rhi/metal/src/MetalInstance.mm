//
// Created on 2026/04/02.
//

#include <MetalInstance.h>
#include <MetalDevice.h>
#include <core/logger/Logger.h>

#import <Metal/Metal.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalInstance::~MetalInstance()
    {
        if (metalDevice != nullptr) {
            [(id<MTLDevice>)metalDevice release];
            metalDevice = nullptr;
        }
    }

    bool MetalInstance::Init(const Instance::Descriptor &desc)
    {
        (void)desc;

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            LOG_E(TAG, "MTLCreateSystemDefaultDevice failed");
            return false;
        }

        [device retain];
        metalDevice = device;

        LOG_I(TAG, "Metal instance initialized: %s", [[device name] UTF8String]);
        return true;
    }

    Device *MetalInstance::CreateDevice()
    {
        auto *device = new MetalDevice(*this);
        if (!device->Init()) {
            delete device;
            return nullptr;
        }
        return device;
    }

} // namespace sky::aurora

extern "C" SKY_EXPORT sky::aurora::Instance::Impl *CreateInstance()
{
    return new sky::aurora::MetalInstance();
}