//
// Created by Zach Lee on 2023/8/31.
//

#include <render/RHI.h>
#include <render/Renderer.h>
#include <render/resource/Texture.h>
#include <rhi/Queue.h>
#include <rhi/Stream.h>

namespace sky {

    Texture::~Texture()
    {
        Renderer::Get()->GetResourceGC()->CollectImageViews(imageView);
    }

    Texture::Texture()
    {
        device = RHI::Get()->GetDevice();
    }

    bool Texture::CheckExtent(uint32_t width, uint32_t height,  uint32_t depth) const
    {
        return imageDesc.extent.width == width && imageDesc.extent.height == height && imageDesc.extent.depth == depth;
    }

    bool TextureCube::Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t mipLevel)
    {
        imageDesc.imageType   = rhi::ImageType::IMAGE_2D;
        imageDesc.format      = format;
        imageDesc.extent      = {width, height, 1};
        imageDesc.mipLevels   = mipLevel;
        imageDesc.arrayLayers = 6;
        imageDesc.samples     = rhi::SampleCount::X1;
        imageDesc.usage       = rhi::ImageUsageFlagBit::TRANSFER_DST | rhi::ImageUsageFlagBit::SAMPLED;
        imageDesc.memory      = rhi::MemoryType::GPU_ONLY;
        imageDesc.cubeCompatible = true;

        image = device->CreateImage(imageDesc);
        if (!image) {
            return false;
        }

        rhi::ImageViewDesc viewDesc = {};
        viewDesc.viewType = rhi::ImageViewType::VIEW_CUBE;
        viewDesc.subRange.levels = mipLevel;
        viewDesc.subRange.layers = 6;
        viewDesc.subRange.aspectMask = rhi::AspectFlagBit::COLOR_BIT;

        imageView = image->CreateView(viewDesc);

        return static_cast<bool>(imageView);
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

    bool Texture3D::Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t depth)
    {
        imageDesc.imageType   = rhi::ImageType::IMAGE_3D;
        imageDesc.format      = format;
        imageDesc.extent      = {width, height, depth};
        imageDesc.mipLevels   = 1;
        imageDesc.arrayLayers = 1;
        imageDesc.samples     = rhi::SampleCount::X1;
        imageDesc.usage       = rhi::ImageUsageFlagBit::TRANSFER_DST | rhi::ImageUsageFlagBit::SAMPLED;
        imageDesc.memory      = rhi::MemoryType::GPU_ONLY;

        image = device->CreateImage(imageDesc);
        if (!image) {
            return false;
        }

        rhi::ImageViewDesc viewDesc = {};
        viewDesc.viewType = rhi::ImageViewType::VIEW_3D;
        viewDesc.subRange.levels = 1;
        viewDesc.subRange.aspectMask = rhi::AspectFlagBit::COLOR_BIT;

        imageView = image->CreateView(viewDesc);

        return static_cast<bool>(imageView);
    }

    void Texture::SetUploadStream(ImageData&& stream)
    {
        data = std::move(stream);
    }

    void Texture::UploadRawData(std::vector<uint8_t> &&rawData, rhi::Queue *queue)
    {
        rhi::ImageUploadRequest request = {};
        request.size   = static_cast<uint32_t>(rawData.size());
        request.source = new rhi::RawBufferStream(std::move(rawData));
        request.imageExtent = imageDesc.extent;

        data.slices.emplace_back(request);
        IStreamableResource::Upload(queue);
    }

    uint64_t Texture::UploadImpl()
    {
        uint64_t size = 0;
        for (auto &req : data.slices) {
            size += req.size;
        }
        uploadHandle = uploadQueue->UploadImage(image, data.slices);
        data.slices.clear();
        return size;
    }
} // namespace sky
