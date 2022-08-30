//
// Created by Zach Lee on 2021/11/7.
//
#pragma once

#include "vulkan/Basic.h"
#include "vulkan/Device.h"
#include "vulkan/vulkan.h"
#include <string>

namespace sky::drv {

    class Driver {
    public:
        struct Descriptor {
            std::string appName;
            std::string engineName;
            bool        enableDebugLayer;
        };

        static Driver *Create(const Descriptor &);
        static void    Destroy(Driver *);

        Device *CreateDevice(const Device::Descriptor &);

        VkInstance GetInstance() const;

    private:
        Driver();
        ~Driver();

        bool Init(const Descriptor &);

        void PrintSupportedExtensions() const;

        VkInstance               instance;
        VkDebugUtilsMessengerEXT debug;
    };

} // namespace sky::drv
