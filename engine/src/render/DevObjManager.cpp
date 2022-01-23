//
// Created by Zach Lee on 2022/1/23.
//


#include <engine/render/DevObjManager.h>

namespace sky {

    void DevObjManager::FreeDeviceObject(drv::DevPtr object)
    {
        std::lock_guard<std::mutex> lock(mutex);
        freeList[currentIndex].emplace_back(object);
    }

    void DevObjManager::TickFreeList()
    {
        currentIndex++;
        currentIndex %= FRAME_NUM;
        std::lock_guard<std::mutex> lock(mutex);
        freeList[currentIndex].clear();
    }
}