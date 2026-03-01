//
// Created by Zach on 2023/2/6.
//

#include <gles/DescriptorSetLayout.h>

namespace sky::gles {


    bool DescriptorSetLayout::Init(const Descriptor &desc)
    {
        descriptorCount = 0;
        bindings = desc.bindings;
        for (auto &binding : bindings) {
            bindingOffsetMap.emplace(binding.binding, descriptorCount);
            descriptorCount += binding.count;
        }

        return true;
    }
}
