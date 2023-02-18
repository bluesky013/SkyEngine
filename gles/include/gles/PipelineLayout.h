//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <rhi/PipelineLayout.h>
#include <gles/DescriptorSetLayout.h>

namespace sky::gles {

    class PipelineLayout : public rhi::PipelineLayout {
    public:
        PipelineLayout() = default;
        ~PipelineLayout() = default;

        bool Init(const Descriptor &desc);
        const std::vector<DescriptorSetLayoutPtr> &GetLayouts() const { return layouts; }

    private:
        std::vector<DescriptorSetLayoutPtr> layouts;
    };

}
