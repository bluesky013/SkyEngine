//
// Created by Zach Lee on 2022/5/7.
//


#include <engine/render/resources/Image.h>
#include <engine/render/DriverManager.h>

namespace sky {

    RDTexturePtr Image::CreateTexture(const Texture::Descriptor& desc)
    {
        auto texture = std::make_shared<Texture>(desc);
        drv::ImageView::Descriptor viewDesc = {};
        viewDesc.subResourceRange.baseMipLevel = desc.baseMipLevel;
        viewDesc.subResourceRange.levelCount = desc.levelCount;
        viewDesc.subResourceRange.baseArrayLayer = desc.baseArrayLayer;
        viewDesc.subResourceRange.layerCount = desc.layerCount;
        auto imageView = rhiImage->CreateImageView(viewDesc);

        drv::Sampler::Descriptor sampDesc = {};
        if (desc.levelCount > 1) {
            sampDesc.maxLod = 13;
        }
        auto sampler = DriverManager::Get()->CreateDeviceObject<drv::Sampler>(sampDesc);

        texture->SetImageView(imageView);
        texture->SetSampler(sampler);
        return texture;
    }

    void Image::InitRHI()
    {
        if (rhiImage) {
            return;
        }

        drv::Image::Descriptor imageDesc = {};
        imageDesc.format = descriptor.format;
        imageDesc.mipLevels = descriptor.mipLevels;
        imageDesc.extent = VkExtent3D{descriptor.extent.width, descriptor.extent.height, 1};

        imageDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        imageDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        imageDesc.arrayLayers = 1;
        imageDesc.imageType = VK_IMAGE_TYPE_2D;
        rhiImage = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::Image>(imageDesc);
    }

    bool Image::IsValid() const
    {
        return !!rhiImage;
    }

    void Image::Update(const uint8_t* data, uint64_t size)
    {
        auto device = DriverManager::Get()->GetDevice();
        auto queue = device->GetQueue({VK_QUEUE_GRAPHICS_BIT});

        drv::Buffer::Descriptor stagingDes = {};
        stagingDes.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
        stagingDes.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        stagingDes.size = size;
        auto stagingBuffer = device->CreateDeviceObject<drv::Buffer>(stagingDes);
        uint8_t* dst = stagingBuffer->Map();
        memcpy(dst, data, size);
        stagingBuffer->UnMap();

        auto cmd = queue->AllocateCommandBuffer({});
        cmd->Begin();

        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        {
            drv::Barrier barrier = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT};
            cmd->ImageBarrier(rhiImage, range, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }

        VkBufferImageCopy imageCopy = {};
        imageCopy.imageSubresource = {range.aspectMask, 0, range.baseArrayLayer, range.layerCount};
        imageCopy.imageExtent = rhiImage->GetImageInfo().extent;
        cmd->Copy(stagingBuffer, rhiImage, imageCopy);

        {
            drv::Barrier barrier = {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
            cmd->ImageBarrier(rhiImage, range, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        cmd->End();
        cmd->Submit(*queue, {});
        cmd->Wait();
    }

}