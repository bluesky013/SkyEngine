//
// Created by Zach Lee on 2022/11/30.
//

#pragma once

#include <vulkan/DevObject.h>
#include <vulkan/Image.h>
#include <vulkan/ImageView.h>

namespace sky::vk {

    class SparseImage : public DevObject {
    public:
        ~SparseImage() = default;

        struct VkDescriptor {
            VkImageType       imageType   = VK_IMAGE_TYPE_2D;
            VkFormat          format      = VK_FORMAT_UNDEFINED;
            VkExtent3D        extent      = {1, 1, 1};
            uint32_t          mipLevels   = 1;
            uint32_t          arrayLayers = 1;
            VkImageUsageFlags usage       = 0;
            VmaMemoryUsage    memory      = VMA_MEMORY_USAGE_GPU_ONLY;
            VkImageViewType   viewType    = VK_IMAGE_VIEW_TYPE_2D;
            VkExtent3D        pageSize    = {1, 1, 1};
        };

        struct VkPage {
            VkOffset3D offset;
            VkExtent3D extent;
        };

        void addPage(const VkPage &page);

    private:
        friend class Device;

        SparseImage(Device &dev) : DevObject(dev) {}

        bool Init(const VkDescriptor &);

        ImagePtr image;
        ImageViewPtr view;
    };

    using SparseImagePtr = std::shared_ptr<SparseImage>;
}
