//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Image.h>
#include <mtl/ImageView.h>
#include <mtl/Device.h>
#include <mtl/Conversion.h>

namespace sky::mtl {

    MTLTextureType GetTextureType(const rhi::Image::Descriptor &desc)
    {
        if (desc.imageType == rhi::ImageType::IMAGE_3D) {
            return MTLTextureType3D;
        }
        bool mulitSample = desc.samples > rhi::SampleCount::X1;
        bool multiLayer = desc.arrayLayers > 1;

        if (multiLayer) {
            return mulitSample ? MTLTextureType2DMultisampleArray : MTLTextureType2DArray;
        }

        return mulitSample ? MTLTextureType2DMultisample : MTLTextureType2D;
    }

    MTLTextureUsage GetTextureUsage(const rhi::ImageUsageFlags &flags)
    {
        MTLTextureUsage usage = MTLTextureUsagePixelFormatView;

        if ((flags & rhi::ImageUsageFlagBit::TRANSFER_SRC) ||
            (flags & rhi::ImageUsageFlagBit::SAMPLED) ||
            (flags & rhi::ImageUsageFlagBit::STORAGE) ||
            (flags & rhi::ImageUsageFlagBit::INPUT_ATTACHMENT)) {
            usage |= MTLTextureUsageShaderRead;
        }

        if ((flags & rhi::ImageUsageFlagBit::STORAGE) ||
            (flags & rhi::ImageUsageFlagBit::TRANSFER_DST)) {
            usage |= MTLTextureUsageShaderWrite;
        }

        if ((flags & rhi::ImageUsageFlagBit::DEPTH_STENCIL) ||
            (flags & rhi::ImageUsageFlagBit::RENDER_TARGET)) {
            usage |= MTLTextureUsageRenderTarget;
        }
        return usage;
    }

    Image::Image(Device &dev) : DevObject(dev)
    {
    }

    Image::~Image()
    {
        if (textureDesc) {
            [textureDesc release];
        }
    }

    bool Image::Init(const Descriptor &desc)
    {
        imageDesc = desc;

        textureDesc = [[MTLTextureDescriptor alloc] init];
        textureDesc.pixelFormat = FromRHI(desc.format);
        textureDesc.width = desc.extent.width;
        textureDesc.height = desc.extent.height;
        textureDesc.depth = desc.extent.depth;
        textureDesc.mipmapLevelCount = desc.mipLevels;
        textureDesc.arrayLength = desc.arrayLayers;
        textureDesc.sampleCount = static_cast<uint32_t>(desc.samples);
        textureDesc.textureType = GetTextureType(desc);
        textureDesc.usage = GetTextureUsage(desc.usage);
        textureDesc.storageMode = FromRHI(desc.usage, desc.memory);

        textureDesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        textureDesc.hazardTrackingMode = MTLHazardTrackingModeDefault;
        texture = [device.GetMetalDevice() newTextureWithDescriptor: textureDesc];
        return true;
    }

    rhi::ImageViewPtr Image::CreateView(const rhi::ImageViewDesc &desc)
    {
        ImageViewPtr ret = std::make_shared<ImageView>(device);
        ret->source      = shared_from_this();
        if (!ret->Init(desc)) {
            ret = nullptr;
        }
        return std::static_pointer_cast<rhi::ImageView>(ret);
    }

    void Image::SetDrawable(id<CAMetalDrawable> drawable)
    {
        [currentDrawable retain];
        currentDrawable = drawable;
    }

    void Image::ResetDrawable()
    {
        [currentDrawable release];
        currentDrawable = nil;
    }


}
