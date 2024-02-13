//
// Created by Zach Lee on 2022/1/18.
//

#pragma once

#include <memory>
#include <vector>
#include <rhi/VertexInput.h>
#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>

namespace sky::vk {

    class VertexInput : public rhi::VertexInput {
    public:
        VertexInput()  = default;
        ~VertexInput() override = default;

        class Builder {
        public:
            Builder()  = default;
            ~Builder() = default;

            Builder &Begin();

            Builder &AddAttribute(uint32_t loc, uint32_t binding, uint32_t off, VkFormat format);

            Builder &AddStream(uint32_t binding, uint32_t stride, VkVertexInputRate);

            std::shared_ptr<VertexInput> Build();

        private:
            std::shared_ptr<VertexInput> vertexInput;
        };

        bool Init(const Descriptor &desc);

        const std::vector<VkVertexInputAttributeDescription> &GetAttributeDescriptions() const { return attributes; }
        const VkPipelineVertexInputStateCreateInfo *GetInfo() const { return &vInputInfo; }
        uint32_t GetHash() const { return hash; }

    private:
        void Build();
        friend class vk::VertexInput::Builder;

        std::vector<VkVertexInputAttributeDescription> attributes;
        std::vector<VkVertexInputBindingDescription>   bindings;
        VkPipelineVertexInputStateCreateInfo           vInputInfo = {};
        uint32_t                                       hash       = 0;
    };
    using VertexInputPtr = std::shared_ptr<VertexInput>;

} // namespace sky::vk
