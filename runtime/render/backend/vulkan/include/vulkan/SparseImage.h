//
// Created by Zach Lee on 2022/11/30.
//

#pragma once

#include <vulkan/DevObject.h>
#include <vulkan/Image.h>
#include <vulkan/ImageView.h>
#include <vulkan/CommandBuffer.h>
#include <core/template/ObjectPool.h>

namespace sky::vk {

    class SparseImage : public DevObject {
    public:
        ~SparseImage();

        struct VkPageInfo {
            VkOffset3D offset = {};
            VkExtent3D extent = {};
            uint32_t layer = 0;
            uint32_t level = 0;
        };

        struct Page : public VkPageInfo {
            VmaAllocation allocation = VK_NULL_HANDLE;
        };

        struct VkDescriptor {
            VkImageType       imageType   = VK_IMAGE_TYPE_2D;
            VkFormat          format      = VK_FORMAT_UNDEFINED;
            VkExtent3D        extent      = {1, 1, 1};
            uint32_t          mipLevels   = 1;
            uint32_t          arrayLayers = 1;
            VkImageUsageFlags usage       = 0;
            VkImageViewType   viewType    = VK_IMAGE_VIEW_TYPE_2D;
        };

        Page* AddPage(const VkPageInfo &info);
        void RemovePage(Page*, bool resetBinding);

        void UpdateBinding();

        bool IsSingleMipTail() const;

        ImagePtr GetImage() const;
        ImageViewPtr GetImageView() const;

    private:
        friend class Device;

        explicit SparseImage(Device &dev) : DevObject(dev) {}

        bool Init(const VkDescriptor &);
        void InitMipTail();

        ImagePtr image;
        ImageViewPtr view;
        VkSparseImageMemoryRequirements sparseMemReq = {};
        VkMemoryRequirements memReq = {};
        VmaPool pool = VK_NULL_HANDLE;
        std::list<Page> pageMemory;

        std::vector<VmaAllocation> mipTailAllocations;
        std::vector<VkSparseMemoryBind> opaqueBinds;
        std::vector<VkSparseImageMemoryBind> sparseImageBinds;
    };

    using SparseImagePtr = std::shared_ptr<SparseImage>;
}
