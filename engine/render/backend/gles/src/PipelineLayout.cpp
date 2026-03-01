//
// Created by Zach Lee on 2023/2/17.
//

#include <gles/PipelineLayout.h>

namespace sky::gles {

    bool PipelineLayout::Init(const Descriptor &desc)
    {
        layouts.reserve(desc.layouts.size());
        for (auto &layout : desc.layouts) {
            layouts.emplace_back(std::static_pointer_cast<DescriptorSetLayout>(layout));
        }
        return true;
    }

}