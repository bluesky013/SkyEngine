//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <mtl/DevObject.h>
#include <mtl/DescriptorSetLayout.h>
#include <rhi/PipelineLayout.h>

namespace sky::mtl {

    class PipelineLayout : public rhi::PipelineLayout, public DevObject {
    public:
        explicit PipelineLayout(Device &dev) : DevObject(dev) {}
        ~PipelineLayout() override = default;

    private:
        friend class Device;
        rhi::DescriptorSetLayoutPtr GetSetLayout(uint32_t set) const override;
        bool Init(const Descriptor &);

        std::vector<DescriptorSetLayoutPtr> desLayouts;
    };
    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;

} // namespace sky::mtl
