//
// Created on 2026/04/02.
//

#include <MetalImage.h>

#include "MetalUtils.h"
#include <MetalDevice.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalImage::MetalImage(MetalDevice &dev) : device(dev)
    {
    }

    MetalImage::~MetalImage()
    {
        Reset();
    }

    const Device *MetalImage::GetDevice() const noexcept
    {
        return &device;
    }

    void MetalImage::Reset()
    {
        if (texture != nullptr && owned) {
            (void)(__bridge_transfer id<MTLTexture>)texture;
        }
        texture = nullptr;
    }

    MetalImage *MetalImage::CreateView(uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount, bool arrayView)
    {
        if (texture == nullptr || !mipCount || !layerCount || (!arrayView && layerCount != 1) || baseMip >= mipLevels ||
            mipCount > mipLevels - baseMip || baseLayer >= arrayLayers || layerCount > arrayLayers - baseLayer)
            return nullptr;
        id<MTLTexture> base = (__bridge id<MTLTexture>)texture;
        if (base.sampleCount > 1 && arrayView && !SupportsMetalFamily7OrMac2(base.device))
            return nullptr;
        const auto     type = base.sampleCount > 1 ? ToMetalMultisampleType(arrayView) : (arrayView ? MTLTextureType2DArray : MTLTextureType2D);
        id<MTLTexture> view = [base newTextureViewWithPixelFormat:base.pixelFormat
                                                      textureType:type
                                                           levels:NSMakeRange(baseMip, mipCount)
                                                           slices:NSMakeRange(baseLayer, layerCount)];
        if (view == nil)
            return nullptr;
        auto *result                     = new MetalImage(device);
        result->descriptor               = descriptor;
        result->descriptor.mipLevels     = mipCount;
        result->descriptor.arrayLayers   = layerCount;
        result->descriptor.extent.width  = std::max(1u, descriptor.extent.width >> baseMip);
        result->descriptor.extent.height = std::max(1u, descriptor.extent.height >> baseMip);
        result->pixelFormat              = pixelFormat;
        result->mipLevels                = mipCount;
        result->arrayLayers              = layerCount;
        result->texture                  = (__bridge_retained void *)view;
        result->owned                    = true;
        return result;
    }

    void MetalImage::RebindBorrowed(void *nativeTexture)
    {
        Reset();
        texture = nativeTexture;
        owned   = false;
    }

    bool MetalImage::Init(const Descriptor &desc, bool forceArray)
    {
        auto *metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for image creation");
            return false;
        }

        descriptor        = desc;
        this->pixelFormat = desc.format;
        this->mipLevels   = desc.mipLevels;
        this->arrayLayers = desc.arrayLayers;

        const auto pixelFormat = ToMetalPixelFormat(desc.format);
        if (!SupportsMetalPixelFormat(metalDevice, desc.format)) {
            LOG_E(TAG, "unsupported Metal pixel format: %u", static_cast<uint32_t>(desc.format));
            return false;
        }
        if (desc.extent.width == 0 || desc.extent.height == 0 || desc.extent.depth == 0) {
            LOG_E(TAG, "invalid image extent: %u x %u x %u", desc.extent.width, desc.extent.height, desc.extent.depth);
            return false;
        }
        const auto &formatInfo = GetImageFormatInfo(desc.format);
        if (formatInfo.isCompressed &&
            (desc.samples != SampleCount::X1 ||
             (desc.usage & (ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::DEPTH_STENCIL | ImageUsageFlagBit::STORAGE)))) {
            LOG_E(TAG, "compressed Metal images require single-sample read-only storage");
            return false;
        }
        if ((forceArray && desc.imageType != ImageType::IMAGE_2D) ||
            (desc.samples != SampleCount::X1 && (desc.imageType != ImageType::IMAGE_2D || desc.extent.depth != 1 || desc.mipLevels != 1 ||
                                                 ![metalDevice supportsTextureSampleCount:ToMetalSampleCount(desc.samples)] ||
                                                 ((desc.arrayLayers > 1 || forceArray) && !SupportsMetalFamily7OrMac2(metalDevice))))) {
            LOG_E(TAG, "unsupported Metal multisample image descriptor");
            return false;
        }

        auto *textureDesc = [[MTLTextureDescriptor alloc] init];
        textureDesc.textureType =
            forceArray ? (desc.samples == SampleCount::X1 ? MTLTextureType2DArray : ToMetalMultisampleType(true)) : ToMetalTextureType(desc);
        textureDesc.pixelFormat      = pixelFormat;
        textureDesc.width            = desc.extent.width;
        textureDesc.height           = desc.extent.height;
        textureDesc.depth            = desc.extent.depth;
        textureDesc.mipmapLevelCount = desc.mipLevels;
        textureDesc.arrayLength      = desc.arrayLayers;
        textureDesc.sampleCount      = ToMetalSampleCount(desc.samples);
        textureDesc.storageMode      = ToMetalStorageMode(desc.usage, desc.memory);
        textureDesc.usage            = ToMetalTextureUsage(desc.usage);

        auto *nativeTexture = [metalDevice newTextureWithDescriptor:textureDesc];

        if (nativeTexture == nil) {
            LOG_E(TAG, "newTextureWithDescriptor failed");
            return false;
        }

        texture = (__bridge_retained void *)nativeTexture;
        owned   = true;

#if SKY_ENABLE_RESOURCE_NAME
        if (desc.name != nullptr) {
            [nativeTexture setLabel:[NSString stringWithUTF8String:desc.name]];
        }
#endif

        return true;
    }

} // namespace sky::aurora
