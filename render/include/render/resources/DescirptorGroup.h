//
// Created by Zach Lee on 2022/5/24.
//


#pragma once

#include <render/resources/RenderResource.h>
#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetLayout.h>

namespace sky {

    class DescriptorGroup : public RenderResource {
    public:
        DescriptorGroup() = default;
        ~DescriptorGroup() = default;

        bool IsValid() const;

    private:
        friend class DescriptorPool;
        drv::DescriptorSetPtr set;
    };
    using RDDesGroupPtr = std::shared_ptr<DescriptorGroup>;

}