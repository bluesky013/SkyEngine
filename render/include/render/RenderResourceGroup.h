//
// Created by Zach Lee on 2023/8/28.
//

#pragma once

#include <rhi/Device.h>
#include <unordered_map>
#include <memory>

namespace sky {

    class ResourceGroupLayout {
    public:
        ~ResourceGroupLayout() = default;

        struct Handler {
            uint32_t binding;
            uint32_t offset;
            uint32_t size;
        };

    private:
        friend class ResourceGroupLayoutBuilder;
        ResourceGroupLayout() = default;
        std::unordered_map<std::string, Handler> handlers;
        rhi::DescriptorSetLayoutPtr layout;
    };

    class ResourceGroupLayoutBuilder {
    public:
        ResourceGroupLayoutBuilder();

        ResourceGroupLayoutBuilder &AddDescriptorType(const std::string &name, rhi::DescriptorType type, rhi::ShaderStageFlags visibility, uint32_t binding, uint32_t count);
        ResourceGroupLayoutBuilder &AddBufferParamsNames(const std::string &name, uint32_t binding, uint32_t size, uint32_t offset);
        ResourceGroupLayout *Build();

    private:
        std::unique_ptr<ResourceGroupLayout> layout;
        rhi::DescriptorSetLayout::Descriptor layoutDesc = {};
    };

    class RenderResourceGroup {
    public:
        RenderResourceGroup() = default;
        ~RenderResourceGroup() = default;

    private:
        rhi::DescriptorSetPtr set;
    };

} // namespace sky