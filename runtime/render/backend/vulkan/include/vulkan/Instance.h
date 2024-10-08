//
// Created by Zach Lee on 2021/11/7.
//
#pragma once

#include "rhi/Instance.h"
#include "vulkan/Basic.h"
#include "vulkan/Device.h"
#include "vulkan/vulkan.h"
#include "vulkan/Ext.h"

namespace sky::vk {
    class Instance : public rhi::Instance {
    public:
        Instance();
        ~Instance() override;

        static Instance *Create(const Descriptor &);
        static void      Destroy(Instance *);

        Device *CreateDevice(const Device::Descriptor &) override;

        VkInstance GetInstance() const;
    private:
        bool Init(const Descriptor &) override;

        VkInstance               instance;
        VkDebugUtilsMessengerEXT debug;
        uint32_t majorVersion = 1;
        uint32_t minorVersion = 0;
    };

    const std::vector<const char *> &GetInstanceExtensions();
    const std::vector<const char *> &GetValidationLayers();

} // namespace sky::vk
