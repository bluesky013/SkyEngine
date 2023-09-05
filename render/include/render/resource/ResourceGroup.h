//
// Created by Zach Lee on 2023/8/28.
//

#pragma once

#include <rhi/Device.h>
#include <unordered_map>
#include <memory>
#include <render/resource/Buffer.h>
#include <render/resource/Texture.h>

namespace sky {

    class ResourceGroupLayout {
    public:
        ResourceGroupLayout() = default;
        ~ResourceGroupLayout() = default;

        struct Handler {
            uint32_t binding;
            uint32_t offset;
            uint32_t size;
        };

    private:
        std::unordered_map<std::string, Handler> handlers;
        rhi::DescriptorSetLayoutPtr layout;
    };
    using RDGResourceLayoutPtr = std::shared_ptr<ResourceGroupLayout>;

    class ResourceGroup {
    public:
        ResourceGroup() = default;
        ~ResourceGroup();

    private:
        rhi::DescriptorSetPtr set;
        RDGResourceLayoutPtr layout;

        std::vector<RDTexturePtr> textures;
        std::vector<RDUniformBufferPtr> uniformBuffers;
        std::vector<RDBufferPtr> storageBuffers;
    };
    using RDResourceGroupPtr = std::shared_ptr<ResourceGroup>;

} // namespace sky
