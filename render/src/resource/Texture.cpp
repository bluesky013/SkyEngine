//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Texture.h>
#include <rhi/Queue.h>
#include <render/RHI.h>
#include <render/Renderer.h>

namespace sky {

    Texture::~Texture()
    {
        imageView = nullptr;
        Renderer::Get()->GetResourceGC()->CollectImage(image);
    }

    Texture::Texture()
    {
        device = RHI::Get()->GetDevice();
    }

    void Texture::Upload(uint8_t *ptr, uint32_t size)
    {
        auto *queue = device->GetQueue(rhi::QueueType::TRANSFER);
        rhi::ImageUploadRequest request = {};
        request.data     = ptr;
        request.offset   = 0;
        request.size     = size;
        request.mipLevel = imageDesc.mipLevels;
        request.layer    = imageDesc.arrayLayers;
        request.imageOffset = {0, 0, 0};
        request.imageExtent = imageDesc.extent;
        queue->UploadImage(image, request);
    }

    bool Texture2D::Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t mipLevel)
    {
        imageDesc.imageType   = rhi::ImageType::IMAGE_2D;
        imageDesc.format      = format;
        imageDesc.extent      = {width, height, 1};
        imageDesc.mipLevels   = mipLevel;
        imageDesc.arrayLayers = 1;
        imageDesc.samples     = rhi::SampleCount::X1;
        imageDesc.usage       = rhi::ImageUsageFlagBit::TRANSFER_DST | rhi::ImageUsageFlagBit::SAMPLED;
        imageDesc.memory      = rhi::MemoryType::GPU_ONLY;

        image = device->CreateImage(imageDesc);
        if (!image) {
            return false;
        }

        rhi::ImageViewDesc viewDesc = {};
        viewDesc.viewType = rhi::ImageViewType::VIEW_2D;
        viewDesc.subRange.levels = mipLevel;
        viewDesc.subRange.aspectMask = rhi::AspectFlagBit::COLOR_BIT;

        imageView = image->CreateView(viewDesc);

        return static_cast<bool>(imageView);
    }

} // namespace sky
