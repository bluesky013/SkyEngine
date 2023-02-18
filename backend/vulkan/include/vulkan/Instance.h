//
// Created by Zach Lee on 2021/11/7.
//
#pragma once

#include "rhi/Instance.h"
#include "vulkan/Basic.h"
#include "vulkan/Device.h"
#include "vulkan/vulkan.h"

namespace sky::vk {

    class Instance : public rhi::Instance {
    public:
        Instance();
        ~Instance();

        static Instance *Create(const Descriptor &);
        static void    Destroy(Instance *);

        Device *CreateDevice(const Device::Descriptor &);

        VkInstance GetInstance() const;

    private:

        bool Init(const Descriptor &);

        void PrintSupportedExtensions() const;

        VkInstance               instance;
        VkDebugUtilsMessengerEXT debug;
    };

} // namespace sky::vk
