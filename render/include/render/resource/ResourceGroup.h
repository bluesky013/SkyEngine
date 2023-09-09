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

        void AddNameHandler(const std::string &key, uint32_t binding);

        std::pair<bool, uint32_t> GetBindingByeName(const std::string &key) const;

        void SetRHILayout(const rhi::DescriptorSetLayoutPtr &layout_) { layout = layout_; }
        const rhi::DescriptorSetLayoutPtr &GetRHILayout() const { return layout; }

    private:
        std::unordered_map<std::string, uint32_t> handlers; // name -> binding
        rhi::DescriptorSetLayoutPtr layout;
    };
    using RDResourceLayoutPtr = std::shared_ptr<ResourceGroupLayout>;

    class ResourceGroup {
    public:
        ResourceGroup() = default;
        ~ResourceGroup();

        void Init(const RDResourceLayoutPtr &layout, rhi::DescriptorSetPool &pool);
        void Update();

        void BindBuffer(const std::string &key, const rhi::BufferPtr &buffer, uint32_t index);
        void BindDynamicUBO(const std::string &key, const RDDynamicUniformBufferPtr &buffer, uint32_t index);
        void BindTexture(const std::string &key, const rhi::ImageViewPtr &view, uint32_t index);
        void BindTexture(const std::string &key, const rhi::ImageViewPtr &view, const rhi::SamplerPtr &sampler, uint32_t index);

        void OnBind(rhi::GraphicsEncoder& encoder, uint32_t setID);
        void OnBind(rhi::ComputeEncoder& encoder, uint32_t setID);

        const RDResourceLayoutPtr &GetLayout() const { return layout; }
        const rhi::DescriptorSetPtr &GetRHISet() const { return set; }
    private:
        rhi::DescriptorSetPtr set;
        RDResourceLayoutPtr   layout;

        std::vector<std::pair<uint32_t, RDDynamicUniformBufferPtr>> dynamicUBOS;
    };
    using RDResourceGroupPtr = std::shared_ptr<ResourceGroup>;

} // namespace sky
