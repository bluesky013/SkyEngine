//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/DescriptorSetLayout.h>
#include <dx12/DevObject.h>

namespace sky::dx {

    class DescriptorSetLayout : public rhi::DescriptorSetLayout, public DevObject {
    public:
        DescriptorSetLayout(Device &dev);
        ~DescriptorSetLayout() override;
    };

}