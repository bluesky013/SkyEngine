//
// Created by Zach on 2023/2/6.
//

#include <gles/DescriptorSet.h>

namespace sky::gles {
    bool DescriptorSet::Init(const Descriptor &desc)
    {
        layout = std::static_pointer_cast<DescriptorSetLayout>(desc.layout);
        descriptors.resize(layout->GetDescriptorCount());
        return true;
    }

    SetDescriptor &DescriptorSet::Get(uint32_t binding, uint32_t index)
    {
        auto &bindingMap = layout->GetBindingMap();

        auto iter = bindingMap.find(binding);
        SKY_ASSERT(iter != bindingMap.end())

#if _DEBUG
        // validator
        auto &bindings = layout->GetBindings();
        auto bIter = std::find_if(bindings.begin(), bindings.end(), [binding](const auto &entry) {
            return entry.binding == binding;
        });
        SKY_ASSERT(bIter != bindings.end());
        SKY_ASSERT(index < bIter->count);
        SKY_ASSERT(iter->second + index < descriptors.size());
#endif
        return descriptors[iter->second + index];
    }

    void DescriptorSet::BindBuffer(uint32_t binding, const rhi::BufferViewPtr &view, uint32_t index)
    {
        auto &desc = Get(binding, index);
        desc.buffer.view = std::static_pointer_cast<BufferView>(view);
    }

    void DescriptorSet::BindImageView(uint32_t binding, const rhi::ImageViewPtr &view, uint32_t index)
    {
        auto &desc = Get(binding, index);
        desc.texture.view = std::static_pointer_cast<ImageView>(view);
    }

    void DescriptorSet::BindSampler(uint32_t binding, const rhi::SamplerPtr &sampler, uint32_t index)
    {
        auto &desc = Get(binding, index);
        desc.texture.sampler = std::static_pointer_cast<Sampler>(sampler);
    }

    void DescriptorSet::Update()
    {
    }
}
