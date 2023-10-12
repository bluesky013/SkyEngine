//
// Created by blues on 2023/10/13.
//

#include <vulkan/PipelineLibrary.h>
#include <core/logger/Logger.h>
#include <vulkan/Device.h>

namespace sky::vk {
    static const char* TAG = "Vulkan";

    PipelineLibrary::~PipelineLibrary()
    {
        if (cache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(device.GetNativeHandle(), cache, VKL_ALLOC);
        }
    }

    bool PipelineLibrary::Init(const Descriptor &desc)
    {
        VkPipelineCacheCreateInfo cacheInfo = {};
        cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        cacheInfo.pNext = nullptr;
        cacheInfo.flags = desc.externalSynchronized ? VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT : 0;
        cacheInfo.initialDataSize = desc.dataSize;
        cacheInfo.pInitialData = desc.data;
        auto res = vkCreatePipelineCache(device.GetNativeHandle(), &cacheInfo, VKL_ALLOC, &cache);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "create buffer failed, %d", res);
            return false;
        }

        return true;
    }

} // namespace sky::vk