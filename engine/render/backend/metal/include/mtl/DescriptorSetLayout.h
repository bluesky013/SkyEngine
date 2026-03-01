//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <mtl/DevObject.h>
#include <rhi/DescriptorSetLayout.h>

namespace sky::mtl {

    class DescriptorSetLayout : public rhi::DescriptorSetLayout, public DevObject {
    public:
        explicit DescriptorSetLayout(Device &dev) : DevObject(dev) {}
        ~DescriptorSetLayout() override;

        bool Init(const Descriptor &);

    private:
        friend class Device;

    };
    using DescriptorSetLayoutPtr = std::shared_ptr<DescriptorSetLayout>;
} // namespace sky::mtl
