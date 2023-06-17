//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <rhi/PipelineLayout.h>
#include <gles/DescriptorSetLayout.h>
#include <gles/DevObject.h>

namespace sky::gles {
    class Device;

    class PipelineLayout : public rhi::PipelineLayout, public DevObject {
    public:
        PipelineLayout(Device &dev) : DevObject(dev) {}
        ~PipelineLayout() = default;

        bool Init(const Descriptor &desc);
        const std::vector<DescriptorSetLayoutPtr> &GetLayouts() const { return layouts; }

    private:
        std::vector<DescriptorSetLayoutPtr> layouts;
    };
    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;
}
