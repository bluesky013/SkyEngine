//
// Created by Zach Lee on 2023/8/18.
//

#pragma once

#include <memory>
#include <rhi/Device.h>

namespace sky {

    class MaterialProxy {
    public:
        MaterialProxy() = default;
        ~MaterialProxy() = default;

        const rhi::DescriptorSetPtr &GetDescriptorSet() const { return set; }

    private:
        rhi::DescriptorSetPtr set;
        rhi::DescriptorSetLayout setLayout;
        std::vector<rhi::BufferPtr> buffer;
        std::vector<rhi::ImageViewPtr> textures;
    };
    using MaterialProxyPtr = std::shared_ptr<MaterialProxy>;

} // namespace sky
