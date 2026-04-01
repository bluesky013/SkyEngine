//
// Created on 2026/04/02.
//

#include <MetalSwapChain.h>
#include <MetalDevice.h>
#include <MetalUtils.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalSwapChain::MetalSwapChain(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalSwapChain::~MetalSwapChain()
    {
        if (layer != nullptr) {
            [(CAMetalLayer *)layer release];
            layer = nullptr;
        }
    }

    bool MetalSwapChain::Init(const Descriptor &desc)
    {
        if (desc.window == nullptr) {
            LOG_E(TAG, "swapchain requires a CAMetalLayer window pointer");
            return false;
        }

        auto *metalDevice = (id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for swapchain creation");
            return false;
        }

        auto *metalLayer = (CAMetalLayer *)desc.window;
        [metalLayer retain];
        metalLayer.device = metalDevice;
        metalLayer.pixelFormat = ToMetalPixelFormat(desc.preferredFormat);
        metalLayer.framebufferOnly = YES;

        layer = metalLayer;
        return true;
    }

} // namespace sky::aurora