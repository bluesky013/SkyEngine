//
// Created by Zach Lee on 2021/11/7.
//
#pragma once

#include "vulkan/vulkan.h"
#include "vulkan/Basic.h"
#include <string>

namespace sky::drv {

    class Driver {
    public:
        struct Descriptor {
            std::string appName;
            std::string engineName;
            bool enableDebugLayer;
        };

        static Driver* Create(const Descriptor&);
        static void Destroy(Driver*);

    private:
        Driver();
        ~Driver();

        bool Init(const Descriptor&);

        VkInstance instance;
        VkDebugUtilsMessengerEXT debug;
    };

}
