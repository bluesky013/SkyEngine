//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <unordered_map>
#include <core/type/Any.h>
#include <render/resource/Texture.h>

namespace sky {

    class Material {
    public:
        Material()  = default;
        ~Material() = default;

        const rhi::DescriptorSetPtr &GetDescriptorSet() const { return set; }

    private:
        rhi::DescriptorSetPtr set;
        rhi::DescriptorSetLayout setLayout;
        std::vector<rhi::BufferPtr> buffer;
        std::vector<rhi::ImageViewPtr> textures;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;
} // namespace sky