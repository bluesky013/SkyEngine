//
// Created by Zach Lee on 2022/5/24.
//

#include <render/resources/DescirptorGroup.h>
#include <render/resources/GlobalResource.h>
#include <vulkan/Util.h>

namespace sky {

    void DescriptorGroup::UpdateTexture(uint32_t binding, const RDTexturePtr &texture)
    {
        auto iter = textures.find(binding);
        if (iter == textures.end()) {
            return;
        }
        iter->second = texture;
        dirty        = true;
    }

    void DescriptorGroup::UpdateBuffer(uint32_t binding, const RDBufferViewPtr &buffer)
    {
        auto iter = buffers.find(binding);
        if (iter == buffers.end()) {
            return;
        }
        iter->second = buffer;
        dirty        = true;
    }

    void DescriptorGroup::Update()
    {
        if (!set || !dirty) {
            return;
        }

        auto writer = set->CreateWriter();

        auto  layout = set->GetLayout();
        auto &table  = layout->GetDescriptorTable();

        auto bindingFn = [&table](uint32_t binding) -> const vk::DescriptorSetLayout::SetBinding * {
            auto iter = table.find(binding);
            if (iter == table.end()) {
                return nullptr;
            }
            return &iter->second;
        };

        for (auto &[binding, buffer] : buffers) {
            auto bindingInfo = bindingFn(binding);
            if (!buffer || !buffer->IsValid() || bindingInfo == nullptr) {
                return;
            }
            writer.Write(binding, bindingInfo->descriptorType, buffer->GetBuffer()->GetRHIBuffer(), buffer->GetOffset(), buffer->GetSize());
        }

        for (auto &[binding, tex] : textures) {
            auto bindingInfo = bindingFn(binding);
            if (!tex || !tex->IsValid() || bindingInfo == nullptr) {
                return;
            }
            writer.Write(binding, bindingInfo->descriptorType, tex->GetImageView(), tex->GetSampler());
        }

        writer.Update();
        dirty = false;
    }

    bool DescriptorGroup::IsValid() const
    {
        return !!set && !dirty;
    }

    vk::DescriptorSetPtr DescriptorGroup::GetRHISet() const
    {
        return set;
    }

    void DescriptorGroup::SetPropertyTable(PropertyTablePtr value)
    {
        properTable = value;
    }

    PropertyTablePtr DescriptorGroup::GetProperTable() const
    {
        return properTable;
    }

    void DescriptorGroup::Init()
    {
        auto  layout = set->GetLayout();
        auto &table  = layout->GetDescriptorTable();
        for (auto &[slot, binding] : table) {
            if (vk::IsBufferDescriptor(binding.descriptorType)) {
                buffers.emplace(slot, nullptr);
            } else if (vk::IsImageDescriptor(binding.descriptorType)) {
                textures.emplace(slot, nullptr);
            }
        }
    }
} // namespace sky
