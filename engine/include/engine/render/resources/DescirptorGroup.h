//
// Created by Zach Lee on 2022/5/24.
//


#pragma once

#include <engine/render/resources/RenderResource.h>
#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetLayout.h>

#include <unordered_map>

namespace sky {

    class DescriptorGroup : public RenderResource {
    public:
        DescriptorGroup() = default;
        ~DescriptorGroup() = default;

        struct BufferEntry {
            uint32_t binding;
            uint32_t offset;
            uint32_t size;
        };

    private:
        drv::DescriptorSetPtr set;
    };
    using RDDesGroupPtr = std::shared_ptr<DescriptorGroup>;

}