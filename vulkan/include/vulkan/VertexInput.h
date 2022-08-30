//
// Created by Zach Lee on 2022/1/18.
//

#pragma once

#include <memory>
#include <vector>
#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>

namespace sky::drv {

    class VertexInput {
    public:
        VertexInput()  = default;
        ~VertexInput() = default;

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

        const std::vector<VkVertexInputAttributeDescription> &GetAttributeDescriptions() const;

        const VkPipelineVertexInputStateCreateInfo *GetInfo() const;

        uint32_t GetHash() const;

    private:
        friend class VertexInput::Builder;

        std::vector<VkVertexInputAttributeDescription> attributes;
        std::vector<VkVertexInputBindingDescription>   bindings;
        VkPipelineVertexInputStateCreateInfo           vInputInfo = {};
        uint32_t                                       hash       = 0;
    };
    using VertexInputPtr = std::shared_ptr<VertexInput>;

} // namespace sky::drv
