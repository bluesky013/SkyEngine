//
// Created by Zach Lee on 2023/8/28.
//

#include <render/resource/ResourceGroup.h>
#include <render/Renderer.h>
#include <core/platform/Platform.h>

namespace sky {

    void ResourceGroupLayout::AddNameHandler(const Name &key, const BindingHandler &handler)
    {
        handlers.emplace(key, handler);
    }

    void ResourceGroupLayout::AddBufferNameHandler(const Name &key, const BufferNameHandler &handler)
    {
        bufferHandlers.emplace(key, handler);
    }

    void ResourceGroupLayout::SetRHILayout(const rhi::DescriptorSetLayoutPtr &layout_)
    {
        layout = layout_;
    }

    const ResourceGroupLayout::BindingHandler *ResourceGroupLayout::GetBindingByeName(const Name &key) const
    {
        auto iter = handlers.find(key);
        if (iter != handlers.end()) {
            return &iter->second;
        }
        return nullptr;
    }

    const ResourceGroupLayout::BufferNameHandler *ResourceGroupLayout::GetBufferMemberByName(const Name &key) const
    {
        auto iter = bufferHandlers.find(key);
        if (iter != bufferHandlers.end()) {
            return &iter->second;
        }
        return nullptr;
    }

    ResourceGroup::~ResourceGroup()
    {
        Renderer::Get()->GetResourceGC()->CollectDescriptorSet(set);
    }

    void ResourceGroup::Init(const RDResourceLayoutPtr &layout_, rhi::DescriptorSetPool &pool)
    {
        layout = layout_;
        set = pool.Allocate({layout->GetRHILayout()});
        SKY_ASSERT(set)

        const auto &defaultRes = Renderer::Get()->GetDefaultResource();

        const auto &bindings = layout->GetRHILayout()->GetBindings();
        for (const auto &binding : bindings) {
            for (uint32_t i = 0; i < binding.count; ++i) {
                if (binding.type == rhi::DescriptorType::SAMPLED_IMAGE) {
                    set->BindImageView(binding.binding, defaultRes.texture2DWhite->GetImageView(), i);
                } else if (binding.type == rhi::DescriptorType::SAMPLER) {
                    set->BindSampler(binding.binding, defaultRes.defaultSampler, i);
                }
            }
        }
    }

    void ResourceGroup::Update()
    {
        set->Update();
    }

    void ResourceGroup::BindBuffer(const Name &key, const rhi::BufferPtr &buffer, uint32_t index)
    {
        const auto *res = layout->GetBindingByeName(key);
        if (res != nullptr) {
            set->BindBuffer(res->binding, buffer, 0, buffer->GetBufferDesc().size, index);
        }
    }

    void ResourceGroup::BindDynamicUBO(const Name &key, const RDDynamicUniformBufferPtr &buffer, uint32_t index)
    {
        const auto *res = layout->GetBindingByeName(key);
        if (res != nullptr) {
            set->BindBuffer(res->binding, buffer->GetRHIBuffer(), 0, buffer->GetSize(), index);
            dynamicUBOS.emplace(layout->GetRHILayout()->GetDescriptorSetOffsetByBinding(res->binding) + index, buffer);
        }
    }

    void ResourceGroup::BindTexture(const Name &key, const rhi::ImageViewPtr &view, uint32_t index)
    {
        const auto *res = layout->GetBindingByeName(key);
        if (res != nullptr) {
            set->BindImageView(res->binding, view, index);
        }
    }

    void ResourceGroup::BindTexture(const Name &key, const rhi::ImageViewPtr &view, const rhi::SamplerPtr &sampler, uint32_t index)
    {
        const auto *res = layout->GetBindingByeName(key);
        if (res != nullptr) {
            set->BindImageView(res->binding, view, index);
            set->BindSampler(res->binding, sampler, index);
        }
    }

    void ResourceGroup::BindSampler(const Name &key, const rhi::SamplerPtr &sampler, uint32_t index)
    {
        const auto *res = layout->GetBindingByeName(key);
        if (res != nullptr) {
            set->BindSampler(res->binding, sampler, index);
        }
    }

    void ResourceGroup::OnBind(rhi::GraphicsEncoder& encoder, uint32_t setID)
    {
        encoder.BindSet(setID, set);
        for (auto &[binding, ubo] : dynamicUBOS) {
            encoder.SetOffset(setID,  binding, 0, static_cast<uint32_t>(ubo->GetOffset()));
        }
    }

    void ResourceGroup::OnBind(rhi::ComputeEncoder& encoder, uint32_t index)
    {
    }

} // namespace sky
