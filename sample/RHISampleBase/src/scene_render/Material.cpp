//
// Created by Zach Lee on 2023/4/7.
//

#include <scene_render/Material.h>
#include <core/util/Overloaded.h>
#include <core/platform/Platform.h>
#include <rhi/Device.h>
#include <IRHI.h>

namespace sky::rhi {

    void Material::SetLayout(const rhi::DescriptorSetLayoutPtr &l, uint32_t size)
    {
        auto *device = Interface<IRHI>::Get()->GetApi()->GetDevice();

        layout = l;
        auto &bindings = layout->GetBindings();
        for (auto &binding : bindings) {
            if (binding.type == rhi::DescriptorType::UNIFORM_BUFFER ||
                binding.type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC) {
                bufferBinding = binding.binding;

                rhi::Buffer::Descriptor bufferDesc = {};
                bufferDesc.size = size;
                bufferDesc.usage = rhi::BufferUsageFlagBit::UNIFORM;
                bufferDesc.memory = rhi::MemoryType::CPU_TO_GPU;
                auto buffer = device->CreateBuffer(bufferDesc);
                buffers.emplace(binding.binding, buffer);
                rawDatas.emplace(binding.binding, RawData(size));
                break;
            }
        }
        batchSet = device->CreateDescriptorSet({layout});
    }

    void Material::AddConnection(const std::string &str, const Accessor &accessor)
    {
        connection.accessors.emplace(str, accessor);
    }

    Material::Accessor Material::GetAccessor(const std::string &key) const
    {
        auto iter = connection.accessors.find(key);
        return iter != connection.accessors.end() ? iter->second : Accessor{~(0U)};
    }

    void Material::SetValue(const std::string &key, const uint8_t *data, uint32_t size)
    {
        auto accessor = GetAccessor(key);
        SKY_ASSERT(accessor.binding != ~(0U))
        SKY_ASSERT(buffers.count(accessor.binding));
        SKY_ASSERT(rawDatas.count(accessor.binding));

        auto &rawData = rawDatas.at(accessor.binding);
        SKY_ASSERT(accessor.offset + size <= rawData.size());
        memcpy(rawData.data() + accessor.offset, data, size);
    }

    void Material::SetTexture(const std::string &key, const Texture &tex)
    {
        auto accessor = GetAccessor(key);
        SKY_ASSERT(accessor.binding != ~(0U))

        textures[accessor.binding] = tex;

        needUpdateSet = true;
    }

    void Material::Update()
    {
        for (auto &[binding, buffer] : buffers) {
            auto &rawData = rawDatas[binding];
            memcpy(buffer->Map(), rawData.data(), rawData.size());
        }

        if (!needUpdateSet) {
            return;
        }
        for (auto &[binding, tex] : textures) {
            batchSet->BindImageView(binding, tex.view, 0);
            batchSet->BindSampler(binding, tex.sampler, 0);
        }
        rhi::BufferViewDesc viewDesc = {};
        for (auto &[binding, buffer] : buffers) {
            viewDesc.range = buffer->GetBufferDesc().size;
            batchSet->BindBuffer(bufferBinding, buffer->CreateView(viewDesc));
        }

        batchSet->Update();
        needUpdateSet = false;
    }
}