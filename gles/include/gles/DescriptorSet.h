//
// Created by Zach on 2023/2/6.
//

#pragma once

#include <rhi/DescriptorSet.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class DescriptorSet : public rhi::DescriptorSet, public DevObject {
    public:
        DescriptorSet(Device &dev) : DevObject(dev) {}
        ~DescriptorSet() = default;
    };

}
