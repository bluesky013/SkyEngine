//
// Created by Zach Lee on 2022/5/29.
//


#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/DescirptorGroup.h>
#include <vulkan/DescriptorSetPool.h>

namespace sky {

    class DescriptorPool : public RenderResource {
    public:
        struct Descriptor {
            uint32_t maxSet = 32;
        };

        DescriptorPool(const Descriptor& desc) : descriptor(desc)
        {
        }

        ~DescriptorPool() = default;

        static std::shared_ptr<DescriptorPool> CreatePool(drv::DescriptorSetLayoutPtr layout, const Descriptor& desc);

        RDDesGroupPtr Allocate();
    private:
        drv::DescriptorSetPoolPtr CreateInternal();

        Descriptor descriptor;
        drv::DescriptorSetLayoutPtr layout;
        std::vector<VkDescriptorPoolSize> sizes;
        std::vector<drv::DescriptorSetPoolPtr> pools;
        uint32_t index = 0;
    };
    using RDDescriptorPoolPtr = std::shared_ptr<DescriptorPool>;
}