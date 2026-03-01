//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <mtl/DevObject.h>
#include <rhi/DescriptorSetPool.h>

namespace sky::mtl {

    class DescriptorSetPool : public rhi::DescriptorSetPool, public DevObject {
    public:
        explicit DescriptorSetPool(Device &dev) : DevObject(dev) {}
        ~DescriptorSetPool() override;

        bool Init(const Descriptor &);

        rhi::DescriptorSetPtr Allocate(const rhi::DescriptorSet::Descriptor &desc) override;

    private:
        friend class Device;
    };

} // namespace sky::mtl
