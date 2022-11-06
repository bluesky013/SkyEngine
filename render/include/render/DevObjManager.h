//
// Created by Zach Lee on 2022/1/23.
//

#pragma once

#include <core/environment/Singleton.h>
#include <list>
#include <mutex>
#include <render/RenderConstants.h>
#include <vulkan/DevObject.h>
#include <vulkan/Device.h>

namespace sky {

    class DevObjManager : public Singleton<DevObjManager> {
    public:
        void FreeDeviceObject(vk::DevPtr object);

        void TickFreeList();

    private:
        friend class Singleton<DevObjManager>;
        DevObjManager()  = default;
        ~DevObjManager() = default;
        std::mutex             mutex;
        uint32_t               currentIndex = 0;
        std::list<vk::DevPtr> freeList[INFLIGHT_FRAME + 1];
    };
} // namespace sky
