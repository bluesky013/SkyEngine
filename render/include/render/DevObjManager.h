//
// Created by Zach Lee on 2022/1/23.
//


#pragma once

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
        static constexpr uint32_t FRAME_NUM = 4;
        uint32_t currentIndex = FRAME_NUM - 1;
        std::list<drv::DevPtr> freeList[FRAME_NUM];
    };
}