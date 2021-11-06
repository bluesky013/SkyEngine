//
// Created by Zach Lee on 2021/11/7.
//

#pragma once
#include "vulkan/vulkan.h"
#include <vector>

namespace sky::drv {

    class Driver;

    class Device {
    public:
        struct Descriptor {
        };

    private:
        bool Init(const Descriptor&, bool enableDebug);

        friend class Driver;
        Device(Driver&);
        ~Device();
        Driver& driver;
        VkPhysicalDevice phyDev;
        VkDevice device;

        VkPhysicalDeviceProperties phyProps;
        VkPhysicalDeviceFeatures phyFeatures;

        std::vector<VkQueueFamilyProperties> queueFamilies;
    };

}
