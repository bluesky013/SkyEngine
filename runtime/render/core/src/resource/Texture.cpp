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
        imageView = nullptr;
        Renderer::Get()->GetResourceGC()->CollectImage(image);
    }

    Texture::Texture()
    {
        device = RHI::Get()->GetDevice();
    }

    rhi::TransferTaskHandle Texture::Upload(const FilePtr &file, rhi::Queue &queue, uint32_t offset)
    {
        rhi::ImageUploadRequest request = {};
        request.source   = std::make_shared<rhi::FileStream>(file, offset);
        request.offset   = 0;
        request.mipLevel = 0;
        request.layer    = 0;
        request.imageOffset = {0, 0, 0};
        request.imageExtent = imageDesc.extent;
        return queue.UploadImage(image, request);
    }

    rhi::TransferTaskHandle Texture::Upload(const std::string &path, rhi::Queue &queue, uint32_t offset)
    {
        rhi::ImageUploadRequest request = {};
        request.source   = std::make_shared<rhi::FileStream>(new NativeFile(path), offset);
        request.offset   = 0;
        request.mipLevel = 0;
        request.layer    = 0;
        request.imageOffset = {0, 0, 0};
        request.imageExtent = imageDesc.extent;
        return queue.UploadImage(image, request);
    }

    rhi::TransferTaskHandle Texture::Upload(const uint8_t *ptr, uint64_t size, rhi::Queue &queue)
    {
        rhi::ImageUploadRequest request = {};
        request.source   = std::make_shared<rhi::RawPtrStream>(ptr);
        request.offset   = 0;
        request.size     = size;
        request.mipLevel = 0;
        request.layer    = 0;
        request.imageOffset = {0, 0, 0};
        request.imageExtent = imageDesc.extent;
        return queue.UploadImage(image, request);
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

} // namespace sky
