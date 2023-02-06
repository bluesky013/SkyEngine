//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <rhi/DescriptorSetLayout.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class DescriptorSetLayout : public rhi::DescriptorSetLayout, public DevObject {
    public:
        DescriptorSetLayout(Device &dev) : DevObject(dev) {}
        ~DescriptorSetLayout() = default;

        bool Init(const Descriptor &desc);
    };

}
