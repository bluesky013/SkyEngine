//
// Created by Zach Lee on 2022/1/23.
//


#pragma once

#include <render/RenderConstants.h>
#include <core/environment/Singleton.h>
#include <vulkan/Device.h>
#include <vulkan/DevObject.h>
#include <list>
#include <mutex>

namespace sky {

    class DevObjManager : public Singleton<DevObjManager> {
    public:
        void FreeDeviceObject(drv::DevPtr object);

        void TickFreeList();

    private:
        friend class Singleton<DevObjManager>;
        DevObjManager() = default;
        ~DevObjManager() = default;
        std::mutex mutex;
        uint32_t currentIndex = 0;
        std::list<drv::DevPtr> freeList[INFLIGHT_FRAME + 1];
    };
}