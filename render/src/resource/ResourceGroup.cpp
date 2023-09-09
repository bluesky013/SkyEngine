//
// Created by Zach Lee on 2023/8/28.
//

#include <render/resource/ResourceGroup.h>
#include <render/Renderer.h>

namespace sky {

    void ResourceGroupLayout::AddNameHandler(const std::string &key, uint32_t binding)
    {
        handlers.emplace(key, binding);
    }

    void ResourceGroupLayout::SetRHILayout(const rhi::DescriptorSetLayoutPtr &layout_)
    {
        layout = layout_;
    }

    std::pair<bool, uint32_t> ResourceGroupLayout::GetBindingByeName(const std::string &key) const
    {
        auto iter = handlers.find(key);
        if (iter != handlers.end()) {
            return {true, iter->second};
        }
        return {false, INVALID_INDEX};
    }

    uint32_t ResourceGroupLayout::GetDescriptorOffsetByBinding(uint32_t binding) const
    {
        return layout->GetDescriptorSetOffsetByBinding(binding);
    }

    ResourceGroup::~ResourceGroup()
    {
        Renderer::Get()->GetResourceGC()->CollectDescriptorSet(set);
    }

    void ResourceGroup::Init(const RDResourceLayoutPtr &layout_, rhi::DescriptorSetPool &pool)
    {
        layout = layout_;
        set = pool.Allocate({layout->GetRHILayout()});

        slotHash.resize(layout->GetRHILayout()->GetDescriptorCount());
    }

    void ResourceGroup::Update()
    {
        set->Update();
    }

    void ResourceGroup::BindBuffer(const std::string &key, const rhi::BufferPtr &buffer, uint32_t index)
    {
        auto res = layout->GetBindingByeName(key);
        if (res.first) {
            set->BindBuffer(res.second, buffer, 0, buffer->GetBufferDesc().size, index);
        }
    }

    void ResourceGroup::BindDynamicUBO(const std::string &key, const RDDynamicUniformBufferPtr &buffer, uint32_t index)
    {
        auto res = layout->GetBindingByeName(key);
        if (res.first) {
            set->BindBuffer(res.second, buffer->GetRHIBuffer(), 0, buffer->GetRange(), index);
            dynamicUBOS.emplace_back(res.second, buffer);
        }
    }

    void ResourceGroup::BindTexture(const std::string &key, const rhi::ImageViewPtr &view, uint32_t index)
    {
        BindTexture(key, view, Renderer::Get()->GetDefaultRHIResource().defaultSampler, index);
    }

    void ResourceGroup::BindTexture(const std::string &key, const rhi::ImageViewPtr &view, const rhi::SamplerPtr &sampler, uint32_t index)
    {
        auto res = layout->GetBindingByeName(key);
        if (res.first) {
            set->BindImageView(res.second, view, index);
        }
    }

    void ResourceGroup::OnBind(rhi::GraphicsEncoder& encoder, uint32_t setID)
    {
        encoder.BindSet(setID, set);
        for (auto &dyn : dynamicUBOS) {
            encoder.SetOffset(setID,  dyn.first, 0, dyn.second->GetOffset());
        }
    }

    void ResourceGroup::OnBind(rhi::ComputeEncoder& encoder, uint32_t index)
    {
    }

} // namespace sky
