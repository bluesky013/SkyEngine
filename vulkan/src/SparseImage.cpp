//
// Created by Zach Lee on 2022/11/30.
//

#include <vulkan/SparseImage.h>
#include <vulkan/Device.h>

namespace sky::vk {

    bool SparseImage::Init(const VkDescriptor &desc)
    {
        Image::VkDescriptor imageDesc = {};
        imageDesc.imageType         = desc.imageType;
        imageDesc.format            = desc.format;
        imageDesc.extent            = desc.extent;
        imageDesc.mipLevels         = desc.mipLevels;
        imageDesc.arrayLayers       = desc.arrayLayers;
        imageDesc.samples           = VK_SAMPLE_COUNT_1_BIT;
        imageDesc.usage             = desc.usage;
        imageDesc.memory            = desc.memory;
        imageDesc.flags             = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
        imageDesc.allocateMem       = false;

        image           = device.CreateDeviceObject<Image>(imageDesc);
        auto &imageInfo = image->GetImageInfo();

        ImageView::VkDescriptor viewDesc = {};
        viewDesc.format                  = imageInfo.format;
        viewDesc.viewType                = desc.viewType;
        viewDesc.subResourceRange        = {0, imageInfo.mipLevels, 0, imageInfo.arrayLayers};

        view = ImageView::CreateImageView(image, viewDesc);

        return image && view;
    }

}
