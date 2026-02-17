//
// Created by Zach Lee on 2022/1/18.
//

#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <vulkan/VertexInput.h>
#include <vulkan/Conversion.h>

namespace sky::vk {

    bool VertexInput::Init(const Descriptor &desc)
    {
        for (uint32_t i = 0; i < desc.attributesNum; ++i) {
            const auto& attribute = desc.attributes[i];
            attributes.emplace_back(VkVertexInputAttributeDescription{attribute.location, attribute.binding, FromRHI(attribute.format), attribute.offset});
        }

        for (uint32_t i = 0; i < desc.bindingsNum; ++i) {
            const auto& binding = desc.bindings[i];
            bindings.emplace_back(VkVertexInputBindingDescription{binding.binding, binding.stride, FromRHI(binding.inputRate)});
        }
        Build();
        return true;
    }

    void VertexInput::Build()
    {
        vInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vInputInfo.flags                           = 0;
        vInputInfo.pNext                           = nullptr;
        vInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vInputInfo.pVertexAttributeDescriptions    = attributes.data();
        vInputInfo.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
        vInputInfo.pVertexBindingDescriptions      = bindings.data();
        hash                                       = 0;
        HashCombine32(hash, Crc32::Cal((uint8_t *)attributes.data(), static_cast<uint32_t>(attributes.size() * sizeof(VkVertexInputAttributeDescription))));
        HashCombine32(hash, Crc32::Cal((uint8_t *)bindings.data(), static_cast<uint32_t>(bindings.size() * sizeof(VkVertexInputBindingDescription))));
    }

    VertexInput::Builder &VertexInput::Builder::Begin()
    {
        vertexInput = std::make_shared<VertexInput>();
        return *this;
    }

    VertexInput::Builder &VertexInput::Builder::AddAttribute(uint32_t loc, uint32_t binding, uint32_t off, VkFormat format)
    {
        vertexInput->attributes.emplace_back(VkVertexInputAttributeDescription{loc, binding, format, off});
        return *this;
    }

    VertexInput::Builder &VertexInput::Builder::AddStream(uint32_t binding, uint32_t stride, VkVertexInputRate rate)
    {
        vertexInput->bindings.emplace_back(VkVertexInputBindingDescription{binding, stride, rate});
        return *this;
    }

    std::shared_ptr<VertexInput> VertexInput::Builder::Build()
    {
        vertexInput->Build();
        return vertexInput;
    }

} // namespace sky::vk
