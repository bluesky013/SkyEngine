//
// Created on 2026/04/02.
//

#include <MetalImage.h>
#include <MetalDevice.h>
#include <MetalUtils.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalImage::MetalImage(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalImage::~MetalImage()
    {
        if (texture != nullptr) {
            [(id<MTLTexture>)texture release];
            texture = nullptr;
        }
    }

    bool MetalImage::Init(const Descriptor &desc)
    {
        auto *metalDevice = (id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for image creation");
            return false;
        }

        const auto pixelFormat = ToMetalPixelFormat(desc.format);
        if (pixelFormat == MTLPixelFormatInvalid) {
            LOG_E(TAG, "unsupported Metal pixel format: %u", static_cast<uint32_t>(desc.format));
            return false;
        }
        if (desc.extent.width == 0 || desc.extent.height == 0 || desc.extent.depth == 0) {
            LOG_E(TAG, "invalid image extent: %u x %u x %u", desc.extent.width, desc.extent.height, desc.extent.depth);
            return false;
        }

        auto *textureDesc = [[MTLTextureDescriptor alloc] init];
        textureDesc.textureType = ToMetalTextureType(desc);
        textureDesc.pixelFormat = pixelFormat;
        textureDesc.width = desc.extent.width;
        textureDesc.height = desc.extent.height;
        textureDesc.depth = desc.extent.depth;
        textureDesc.mipmapLevelCount = desc.mipLevels;
        textureDesc.arrayLength = desc.arrayLayers;
        textureDesc.sampleCount = ToMetalSampleCount(desc.samples);
        textureDesc.storageMode = ToMetalStorageMode(desc.usage, desc.memory);
        textureDesc.usage = ToMetalTextureUsage(desc.usage);

        auto *nativeTexture = [metalDevice newTextureWithDescriptor:textureDesc];
        [textureDesc release];

        if (nativeTexture == nil) {
            LOG_E(TAG, "newTextureWithDescriptor failed");
            return false;
        }

        texture = nativeTexture;
        return true;
    }

} // namespace sky::aurora