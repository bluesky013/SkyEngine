//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Image.h"
#include "core/logger/Logger.h"
#include "rhi/Decode.h"
#include "vulkan/Basic.h"
#include "vulkan/Device.h"
#include "vulkan/Conversion.h"
#include "vk_mem_alloc.h"

static const char *TAG = "Vulkan";

namespace sky::vk {

    std::unordered_map<VkFormat, rhi::ImageFormatInfo> FORMAT_INFO =
        {
            {VK_FORMAT_R8G8B8A8_UNORM,            {4, 1, 1, false}},
            {VK_FORMAT_R8G8B8A8_SRGB,             {4, 1, 1, false}},
            {VK_FORMAT_B8G8R8A8_UNORM,            {4, 1, 1, false}},
            {VK_FORMAT_B8G8R8A8_SRGB,             {4, 1, 1, false}},
            {VK_FORMAT_BC1_RGB_UNORM_BLOCK,       {8, 4, 4, true}},
            {VK_FORMAT_BC1_RGB_SRGB_BLOCK,        {8, 4, 4, true}},
            {VK_FORMAT_BC1_RGBA_UNORM_BLOCK,      {8, 4, 4, true}},
            {VK_FORMAT_BC1_RGBA_SRGB_BLOCK,       {8, 4, 4, true}},
            {VK_FORMAT_BC2_UNORM_BLOCK,           {16, 4, 4, true}},
            {VK_FORMAT_BC2_SRGB_BLOCK,            {16, 4, 4, true}},
            {VK_FORMAT_BC3_UNORM_BLOCK,           {16, 4, 4, true}},
            {VK_FORMAT_BC3_SRGB_BLOCK,            {16, 4, 4, true}},
            {VK_FORMAT_BC4_UNORM_BLOCK,           {8, 4, 4, true}},
            {VK_FORMAT_BC4_SNORM_BLOCK,           {8, 4, 4, true}},
            {VK_FORMAT_BC5_UNORM_BLOCK,           {16, 4, 4, true}},
            {VK_FORMAT_BC5_SNORM_BLOCK,           {16, 4, 4, true}},
            {VK_FORMAT_BC6H_UFLOAT_BLOCK,         {16, 4, 4, true}},
            {VK_FORMAT_BC6H_SFLOAT_BLOCK,         {16, 4, 4, true}},
            {VK_FORMAT_BC7_UNORM_BLOCK,           {16, 4, 4, true}},
            {VK_FORMAT_BC7_SRGB_BLOCK,            {16, 4, 4, true}},
            {VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,   {8, 4, 4, true}},
            {VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,    {8, 4, 4, true}},
            {VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, {8, 4, 4, true}},
            {VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,  {8, 4, 4, true}},
            {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, {16, 4, 4, true}},
            {VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,  {16, 4, 4, true}},
            {VK_FORMAT_ASTC_4x4_UNORM_BLOCK  ,    {16, 4, 4, true}},
            {VK_FORMAT_ASTC_4x4_SRGB_BLOCK   ,    {16, 4, 4, true}},
            {VK_FORMAT_ASTC_8x8_UNORM_BLOCK  ,    {16, 8,  8,  true}},
            {VK_FORMAT_ASTC_8x8_SRGB_BLOCK   ,    {16, 8,  8,  true}},
            {VK_FORMAT_ASTC_10x10_UNORM_BLOCK,    {16, 10, 10, true}},
            {VK_FORMAT_ASTC_10x10_SRGB_BLOCK ,    {16, 10, 10, true}},
            {VK_FORMAT_ASTC_12x12_UNORM_BLOCK,    {16, 12, 12, true}},
            {VK_FORMAT_ASTC_12x12_SRGB_BLOCK ,    {16, 12, 12, true}},
    };

    rhi::ImageFormatInfo *GetImageInfoByFormat(VkFormat format)
    {
        auto iter = FORMAT_INFO.find(format);
        return iter == FORMAT_INFO.end() ? nullptr : &iter->second;
    }

    Image::Image(Device &dev) : DevObject(dev), image(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), imageInfo{}
    {
    }

    Image::Image(Device &dev, VkImage img) : DevObject(dev), image(img), allocation(VK_NULL_HANDLE), imageInfo{}, isOwn(false)
    {
    }

    Image::~Image()
    {
        Reset();
    }

    bool Image::Init(const Descriptor &des)
    {
        imageDesc = des;

        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.mipLevels     = des.mipLevels;
        imageInfo.arrayLayers   = des.arrayLayers;
        imageInfo.format        = FromRHI(des.format);
        imageInfo.extent        = FromRHI(des.extent);
        imageInfo.imageType     = FromRHI(des.imageType);
        imageInfo.usage         = FromRHI(des.usage);
        imageInfo.samples       = static_cast<VkSampleCountFlagBits>(des.samples);
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageFormatProperties properties{};
        auto res = vkGetPhysicalDeviceImageFormatProperties(device.GetGpuHandle(), imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage, imageInfo.flags, &properties);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "image not supported %d", res);
        }

        if (des.allocateMem) {
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage                   = FromRHI(des.memory);

            res = vmaCreateImage(device.GetAllocator(), &imageInfo, &allocInfo, &image, &allocation, nullptr);
        } else {
            res = vkCreateImage(device.GetNativeHandle(), &imageInfo, nullptr, &image);
        }
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "create image failed %d", res);
            return false;
        }

        auto *tmpInfo = rhi::GetImageInfoByFormat(imageDesc.format);
        if (tmpInfo != nullptr) {
            formatInfo = *tmpInfo;
        }
        return true;
    }

    bool Image::Init(const VkDescriptor &des)
    {
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.flags         = des.flags;
        imageInfo.mipLevels     = des.mipLevels;
        imageInfo.arrayLayers   = des.arrayLayers;
        imageInfo.format        = des.format;
        imageInfo.extent        = des.extent;
        imageInfo.imageType     = des.imageType;
        imageInfo.usage         = des.usage;
        imageInfo.samples       = des.samples;
        imageInfo.tiling        = des.tiling;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageFormatProperties properties{};
        auto res = vkGetPhysicalDeviceImageFormatProperties(device.GetGpuHandle(), imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage, imageInfo.flags, &properties);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "image not supported %d", res);
        }

        if (des.allocateMem) {
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage                   = des.memory;

            res = vmaCreateImage(device.GetAllocator(), &imageInfo, &allocInfo, &image, &allocation, nullptr);
        } else {
            res = vkCreateImage(device.GetNativeHandle(), &imageInfo, nullptr, &image);
        }
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "create image failed %d", res);
            return false;
        }

        auto *tmpInfo = GetImageInfoByFormat(imageInfo.format);
        if (tmpInfo != nullptr) {
            formatInfo = *tmpInfo;
        }
        return true;
    }

    void Image::Reset()
    {
        if (image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(device.GetAllocator(), image, allocation);
        } else if (image != VK_NULL_HANDLE && isOwn) {
            vkDestroyImage(device.GetNativeHandle(), image, VKL_ALLOC);
        }
        image      = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }

    bool Image::IsTransient() const
    {
        return allocation == VK_NULL_HANDLE;
    }

    const VkImageCreateInfo &Image::GetImageInfo() const
    {
        return imageInfo;
    }

    VkImage Image::GetNativeHandle() const
    {
        return image;
    }

    void Image::BindMemory(VmaAllocation alloc)
    {
        allocation = alloc;
        vmaBindImageMemory(device.GetAllocator(), allocation, image);
    }

    void Image::ReleaseMemory()
    {
        vmaFreeMemory(device.GetAllocator(), allocation);
        allocation = VK_NULL_HANDLE;
    }
} // namespace sky::vk
