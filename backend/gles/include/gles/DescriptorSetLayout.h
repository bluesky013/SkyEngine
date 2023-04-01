//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <rhi/DescriptorSetLayout.h>
#include <gles/DevObject.h>
#include <unordered_map>

namespace sky::gles {

    class DescriptorSetLayout : public rhi::DescriptorSetLayout, public DevObject {
    public:
        DescriptorSetLayout(Device &dev) : DevObject(dev) {}
        ~DescriptorSetLayout() = default;

        bool Init(const Descriptor &desc);
        const std::vector<SetBinding> &GetBindings() const { return bindings; }
        const std::unordered_map<uint32_t, uint32_t> &GetBindingMap() const { return bindingOffsetMap; }

    private:
        std::unordered_map<uint32_t, uint32_t> bindingOffsetMap;
        std::vector<SetBinding> bindings;
    };
    using DescriptorSetLayoutPtr = std::shared_ptr<DescriptorSetLayout>;

}
