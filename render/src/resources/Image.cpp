//
// Created by Zach Lee on 2022/5/7.
//

#include <cereal/archives/binary.hpp>
#include <framework/asset/AssetManager.h>
#include <render/DriverManager.h>
#include <render/resources/Image.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace sky {
    void Image::InitRHI()
    {
        if (rhiImage) {
            return;
        }

        drv::Image::Descriptor imageDesc = {};
        imageDesc.format                 = descriptor.format;
        imageDesc.mipLevels              = descriptor.mipLevels;
        imageDesc.extent                 = VkExtent3D{descriptor.extent.width, descriptor.extent.height, 1};

        imageDesc.usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.memory      = VMA_MEMORY_USAGE_GPU_ONLY;
        imageDesc.samples     = VK_SAMPLE_COUNT_1_BIT;
        imageDesc.arrayLayers = 1;
        imageDesc.imageType   = VK_IMAGE_TYPE_2D;
        rhiImage              = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::Image>(imageDesc);
    }

    bool Image::IsValid() const
    {
        return !!rhiImage;
    }

    VkFormat Image::GetFormat() const
    {
        return descriptor.format;
    }

    drv::ImagePtr Image::GetRHIImage() const
    {
        return rhiImage;
    }

    void Image::Update(const uint8_t *data, uint64_t size)
    {
        auto device = DriverManager::Get()->GetDevice();
        auto queue  = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);

        drv::Buffer::Descriptor stagingDes = {};
        stagingDes.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
        stagingDes.usage                   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        stagingDes.size                    = size;
        auto     stagingBuffer             = device->CreateDeviceObject<drv::Buffer>(stagingDes);
        uint8_t *dst                       = stagingBuffer->Map();
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
        imageCopy.imageSubresource  = {range.aspectMask, 0, range.baseArrayLayer, range.layerCount};
        imageCopy.imageExtent       = rhiImage->GetImageInfo().extent;
        cmd->Copy(stagingBuffer, rhiImage, imageCopy);

        {
            drv::Barrier barrier = {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_ACCESS_SHADER_READ_BIT};
            cmd->ImageBarrier(rhiImage, range, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        cmd->End();
        cmd->Submit(*queue, {});
        cmd->Wait();
    }

    RDImagePtr Image::CreateFromData(const ImageAssetData &data)
    {
        return {};
    }

    RDImagePtr CreateImage2D()
    {
        static const uint8_t data[] = {127, 127, 127, 255, 255, 255, 255, 255, 255, 255, 255, 255, 127, 127, 127, 255};

        Image::Descriptor desc = {};
        desc.format            = VK_FORMAT_R8G8B8A8_UNORM;
        desc.extent            = {2, 2};

        auto image = std::make_shared<Image>(desc);
        image->InitRHI();
        image->Update(data, sizeof(data));

        return image;
    }
} // namespace sky