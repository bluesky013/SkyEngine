//
// Created by Zach Lee on 2023/8/28.
//

#pragma once

#include <rhi/Device.h>
#include <unordered_map>
#include <memory>
#include <render/resource/Buffer.h>
#include <render/resource/Texture.h>
#include <render/RenderResource.h>

namespace sky {
    static constexpr uint32_t PASS_SET = 0;
    static constexpr uint32_t BATCH_SET = 1;
    static constexpr uint32_t INSTANCE_SET = 2;

    class ResourceGroupLayout : public RenderResource {
    public:
        ResourceGroupLayout() = default;
        ~ResourceGroupLayout() override = default;

        struct BufferNameHandler {
            uint32_t binding;
            uint32_t offset;
            uint32_t size;
        };

        struct BindingHandler {
            uint32_t binding;
            uint32_t size;
        };

        void AddNameHandler(const Name &key, const BindingHandler &handler);
        void AddBufferNameHandler(const Name &key, const BufferNameHandler &nameHandler);
        void SetRHILayout(const rhi::DescriptorSetLayoutPtr &layout);

        const BindingHandler *GetBindingByeName(const Name &key) const;
        const BufferNameHandler *GetBufferMemberByName(const Name &key) const;
        const std::unordered_map<Name, BindingHandler> &GetBindingHandlers() const { return handlers; }
        const std::unordered_map<Name, BufferNameHandler> &GetBufferNameHandles() const { return bufferHandlers; }
        const rhi::DescriptorSetLayoutPtr &GetRHILayout() const { return layout; }

        bool IsEmpty() const { return handlers.empty(); }

    private:
        std::unordered_map<Name, BindingHandler> handlers;    // name -> [binding, size]
        std::unordered_map<Name, BufferNameHandler> bufferHandlers; // name -> buffer name handler
        rhi::DescriptorSetLayoutPtr layout;
    };
    using RDResourceLayoutPtr = CounterPtr<ResourceGroupLayout>;

    class ResourceGroup : public RenderResource {
    public:
        ResourceGroup() = default;
        ~ResourceGroup() override;

        void Init(const RDResourceLayoutPtr &layout, rhi::DescriptorSetPool &pool);
        void Update();

        void BindBuffer(const Name &key, const rhi::BufferPtr &buffer, uint32_t index);
        void BindDynamicUBO(const Name &key, const RDDynamicUniformBufferPtr &buffer, uint32_t index);
        void BindTexture(const Name &key, const rhi::ImageViewPtr &view, uint32_t index);
        void BindTexture(const Name &key, const rhi::ImageViewPtr &view, const rhi::SamplerPtr &sampler, uint32_t index);
        void BindSampler(const Name &key, const rhi::SamplerPtr &sampler, uint32_t index);

        void OnBind(rhi::GraphicsEncoder &encoder, uint32_t setID);
        void OnBind(rhi::ComputeEncoder &encoder, uint32_t setID);

        const RDResourceLayoutPtr &GetLayout() const { return layout; }
        const rhi::DescriptorSetPtr &GetRHISet() const { return set; }
    private:
        rhi::DescriptorSetPtr set;
        RDResourceLayoutPtr   layout;

        std::unordered_map<uint32_t, RDDynamicUniformBufferPtr> dynamicUBOS;
    };
    using RDResourceGroupPtr = CounterPtr<ResourceGroup>;

} // namespace sky
