//
// Created by Zach Lee on 2023/5/13.
//

#pragma once

#include <rhi/DescriptorSetPool.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class DescriptorSetPool : public rhi::DescriptorSetPool, public DevObject, public std::enable_shared_from_this<DescriptorSetPool> {
    public:
        explicit DescriptorSetPool(Device &dev) : DevObject(dev) {}
        ~DescriptorSetPool() = default;


        virtual rhi::DescriptorSetPtr Allocate(const rhi::DescriptorSet::Descriptor &desc) override;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);
    };
    using DescriptorSetPoolPtr = std::shared_ptr<DescriptorSetPool>;

} // namespace sky::gles