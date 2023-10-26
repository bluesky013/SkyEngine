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
        explicit PipelineLayout(Device &dev) : DevObject(dev) {}
        ~PipelineLayout() override = default;

        bool Init(const Descriptor &desc);
        const std::vector<DescriptorSetLayoutPtr> &GetLayouts() const { return layouts; }
        rhi::DescriptorSetLayoutPtr GetSetLayout(uint32_t set) const override
        {
            return std::static_pointer_cast<DescriptorSetLayout>(layouts[set]);
        }

    private:
        std::vector<DescriptorSetLayoutPtr> layouts;
    };
    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;
}
