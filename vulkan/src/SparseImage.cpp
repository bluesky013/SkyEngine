//
// Created by Zach Lee on 2022/11/30.
//

#include <vulkan/SparseImage.h>
#include <vulkan/Device.h>
#include <core/math/Util.h>

namespace sky::vk {

    static bool Check1D(uint32_t length, int32_t offset, uint32_t total, uint32_t granularity)
    {
        return length % granularity == 0 ? true : (length + offset) == total;
    }

    static bool CheckPage(const VkExtent3D &extent, const SparseImage::VkPageInfo &pageExtent, const VkExtent3D &imageGranularity)
    {
        return Check1D(pageExtent.extent.width, pageExtent.offset.x, extent.width, imageGranularity.width) &&
               Check1D(pageExtent.extent.height, pageExtent.offset.y, extent.height, imageGranularity.height) &&
               Check1D(pageExtent.extent.depth, pageExtent.offset.z, extent.depth, imageGranularity.depth);
    }

    SparseImage::~SparseImage()
    {
        image = nullptr;
        view = nullptr;

        auto allocator = device.GetAllocator();
        for (auto &page : pageMemory) {
            vmaFreeMemoryPages(allocator, 1, &page.allocation);
        }

        for (auto &mipTail : mipTailAllocations) {
            vmaFreeMemoryPages(allocator, 1, &mipTail);
        }

        if (pool != VK_NULL_HANDLE) {
            vmaDestroyPool(allocator, pool);
        }
    }

    bool SparseImage::Init(const VkDescriptor &desc)
    {
        Image::VkDescriptor imageDesc = {};
        VkPhysicalDeviceSparseImageFormatInfo2 sparseInfo = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2};

        imageDesc.imageType   = sparseInfo.type    = desc.imageType;
        imageDesc.format      = sparseInfo.format  = desc.format;
        imageDesc.usage       = sparseInfo.usage   = desc.usage | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
        imageDesc.samples     = sparseInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageDesc.tiling      = sparseInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;

        imageDesc.extent      = desc.extent;
        imageDesc.mipLevels   = desc.mipLevels;
        imageDesc.arrayLayers = desc.arrayLayers;
        imageDesc.memory      = VMA_MEMORY_USAGE_GPU_ONLY;
        imageDesc.flags       = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
        imageDesc.allocateMem = false;

        image           = device.CreateDeviceObject<Image>(imageDesc);
        auto &imageInfo = image->GetImageInfo();

        vkGetImageMemoryRequirements(device.GetNativeHandle(), image->GetNativeHandle(), &memReq);

        uint32_t count = 0;
        vkGetImageSparseMemoryRequirements(device.GetNativeHandle(), image->GetNativeHandle(), &count, nullptr);
        std::vector<VkSparseImageMemoryRequirements> reqs;
        reqs.resize(count, {});
        vkGetImageSparseMemoryRequirements(device.GetNativeHandle(), image->GetNativeHandle(), &count, reqs.data());
        sparseMemReq = reqs.front();

        if (sparseMemReq.imageMipTailFirstLod < imageInfo.mipLevels) {
            InitMipTail();
        }

        ImageView::VkDescriptor viewDesc = {};
        viewDesc.format                  = imageInfo.format;
        viewDesc.viewType                = desc.viewType;
        viewDesc.subResourceRange        = {sparseMemReq.formatProperties.aspectMask, 0, imageInfo.mipLevels, 0, imageInfo.arrayLayers};
        view = ImageView::CreateImageView(image, viewDesc);

        VmaPoolCreateInfo poolInfo = {};
        poolInfo.memoryTypeIndex = device.FindProperties(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vmaCreatePool(device.GetAllocator(), &poolInfo, &pool);

        return image && view;
    }

    void SparseImage::InitMipTail()
    {
        auto allocator = device.GetAllocator();
        auto allocateFn = [this, allocator](VkDeviceSize offset) {
            VmaAllocationCreateInfo allocationCreateInfo = {};
            allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            allocationCreateInfo.pool = pool;

            VkMemoryRequirements requirements = {};
            requirements.size           = sparseMemReq.imageMipTailSize;
            requirements.alignment      = memReq.alignment;
            requirements.memoryTypeBits = memReq.memoryTypeBits;

            VmaAllocation allocation = VK_NULL_HANDLE;
            VmaAllocationInfo allocationInfo = {};
            vmaAllocateMemoryPages(allocator, &requirements, &allocationCreateInfo, 1, &allocation, &allocationInfo);

            VkSparseMemoryBind bind = {};
            bind.resourceOffset = offset;
            bind.size           = allocationInfo.size;
            bind.memory         = allocationInfo.deviceMemory;
            bind.memoryOffset   = allocationInfo.offset;

            mipTailAllocations.emplace_back(allocation);
            opaqueBinds.emplace_back(bind);
        };

        if (IsSingleMipTail()) {
            allocateFn(sparseMemReq.imageMipTailOffset);
            return;
        }

        for (uint32_t i = 0; i < image->GetImageInfo().arrayLayers; ++i) {
            if (!IsSingleMipTail()) {
                allocateFn(sparseMemReq.imageMipTailOffset + i * sparseMemReq.imageMipTailStride);
            }
        }
    }

    SparseImage::Page* SparseImage::AddPage(const VkPageInfo &info)
    {
        auto extent = image->GetImageInfo().extent;
        extent.width  = std::max(extent.width >> info.level, 1U);
        extent.height = std::max(extent.height >> info.level, 1U);
        extent.depth  = std::max(extent.depth >> info.level, 1U);

        if (!CheckPage(extent, info, sparseMemReq.formatProperties.imageGranularity)) {
            return nullptr;
        }

        uint32_t nWidth = AlignDivision(extent.width, sparseMemReq.formatProperties.imageGranularity.width);
        uint32_t nHeight = AlignDivision(extent.height, sparseMemReq.formatProperties.imageGranularity.height);
        uint32_t nDepth = AlignDivision(extent.depth, sparseMemReq.formatProperties.imageGranularity.depth);

        auto &page = pageMemory.emplace_back();
        page.allocation =  VK_NULL_HANDLE;
        page.extent = info.extent;
        page.offset = info.offset;
        page.level = info.level;
        page.layer = info.layer;

        auto allocator = device.GetAllocator();
        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocationCreateInfo.pool = pool;

        VkMemoryRequirements requirements = {};
        requirements.size           = nWidth * nHeight * nDepth * memReq.alignment;
        requirements.alignment      = memReq.alignment;
        requirements.memoryTypeBits = memReq.memoryTypeBits;

        VmaAllocationInfo allocationInfo = {};
        vmaAllocateMemoryPages(allocator, &requirements, &allocationCreateInfo, 1, &page.allocation, &allocationInfo);

        sparseImageBinds.emplace_back(VkSparseImageMemoryBind{});
        VkSparseImageMemoryBind &imageMemoryBind = sparseImageBinds.back();
        imageMemoryBind.subresource = {sparseMemReq.formatProperties.aspectMask, info.level, info.layer};
        imageMemoryBind.offset = info.offset;
        imageMemoryBind.extent = info.extent;
        imageMemoryBind.memory = allocationInfo.deviceMemory;
        imageMemoryBind.memoryOffset = allocationInfo.offset;
        return &page;
    }

    void SparseImage::RemovePage(Page* page, bool resetBinding)
    {
        auto iter = std::find_if(pageMemory.begin(), pageMemory.end(), [page](const Page &p) {
            return &p == page;
        });
        if (iter == pageMemory.end()) {
            return;
        }
        vmaFreeMemoryPages(device.GetAllocator(), 1, &page->allocation);

        if (resetBinding) {
            sparseImageBinds.emplace_back(VkSparseImageMemoryBind{});
            VkSparseImageMemoryBind &imageMemoryBind = sparseImageBinds.back();
            imageMemoryBind.subresource = {sparseMemReq.formatProperties.aspectMask, page->level, page->layer};
            imageMemoryBind.offset = page->offset;
            imageMemoryBind.extent = page->extent;
            imageMemoryBind.memory = VK_NULL_HANDLE;
            imageMemoryBind.memoryOffset = 0;
        }
        pageMemory.erase(iter);
    }

    bool SparseImage::IsSingleMipTail() const
    {
        return (sparseMemReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) == VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
    }

    void SparseImage::UpdateBinding()
    {
        VkBindSparseInfo sparseBinding = {};
        sparseBinding.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;

        VkSparseImageOpaqueMemoryBindInfo opaqueMemoryBindInfo = {};
        if (!opaqueBinds.empty()) {
            opaqueMemoryBindInfo.image = image->GetNativeHandle();
            opaqueMemoryBindInfo.bindCount = static_cast<uint32_t>(opaqueBinds.size());
            opaqueMemoryBindInfo.pBinds = opaqueBinds.data();
            sparseBinding.imageOpaqueBindCount = 1;
            sparseBinding.pImageOpaqueBinds = &opaqueMemoryBindInfo;
        }

        VkSparseImageMemoryBindInfo imageMemoryBindInfo = {};
        if (!sparseImageBinds.empty()) {
            imageMemoryBindInfo.image = image->GetNativeHandle();
            imageMemoryBindInfo.bindCount = static_cast<uint32_t>(sparseImageBinds.size());
            imageMemoryBindInfo.pBinds = sparseImageBinds.data();
            sparseBinding.imageBindCount = 1;
            sparseBinding.pImageBinds = &imageMemoryBindInfo;
        }
        vkQueueBindSparse(device.GetAsyncTransferQueue()->GetQueue()->GetNativeHandle(), 1, &sparseBinding, VK_NULL_HANDLE);

        sparseImageBinds.clear();
        opaqueBinds.clear();

        // TODO : Sync.
        device.WaitIdle();
    }

    ImagePtr SparseImage::GetImage() const
    {
        return image;
    }

    ImageViewPtr SparseImage::GetImageView() const
    {
        return view;
    }

}
