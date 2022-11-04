//
// Created by Zach Lee on 2022/1/23.
//

#include <render/DevObjManager.h>

namespace sky {

    void DevObjManager::FreeDeviceObject(vk::DevPtr object)
    {
        std::lock_guard<std::mutex> lock(mutex);
        freeList[currentIndex].emplace_back(object);
    }

    void DevObjManager::TickFreeList()
    {
        std::lock_guard<std::mutex> lock(mutex);
        currentIndex = (currentIndex + 1) % (INFLIGHT_FRAME + 1);
        freeList[currentIndex].clear();
    }
} // namespace sky
