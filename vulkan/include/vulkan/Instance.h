//
// Created by Zach Lee on 2021/11/7.
//
#pragma once

#include "vulkan/Basic.h"
#include "vulkan/Device.h"
#include "vulkan/vulkan.h"
#include <string>

namespace sky::vk {

    class Instance {
    public:
        struct Descriptor {
            std::string appName;
            std::string engineName;
            bool        enableDebugLayer;
        };

        static Instance *Create(const Descriptor &);
        static void    Destroy(Instance *);

        Device *CreateDevice(const Device::Descriptor &);

        VkInstance GetInstance() const;

    private:
        Instance();
        ~Instance();

        bool Init(const Descriptor &);

        void PrintSupportedExtensions() const;

        VkInstance               instance;
        VkDebugUtilsMessengerEXT debug;
    };

} // namespace sky::vk
