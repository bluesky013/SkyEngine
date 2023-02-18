//
// Created by Zach Lee on 2022/7/31.
//

#pragma once

#include <vector>
#include <vulkan/Buffer.h>
#include <vulkan/VertexInput.h>

namespace sky::vk {

    class VertexAssembly {
    public:
        VertexAssembly()  = default;
        ~VertexAssembly() = default;

        void SetVertexInput(VertexInputPtr input);

        void ResetVertexBuffer();

        void AddVertexBuffer(const BufferPtr &buffer, VkDeviceSize offset = 0);

        void SetIndexBuffer(const BufferPtr &buffer, VkDeviceSize offset = 0);

        void SetIndexType(VkIndexType);

        void OnBind(VkCommandBuffer) const;

        bool IsIndexed() const;

    private:
        VertexInputPtr            vertexInput;
        std::vector<BufferPtr>    vertexBuffers;
        std::vector<VkBuffer>     vkBuffers;
        std::vector<VkDeviceSize> offsets;
        BufferPtr                 indexBuffer;
        VkDeviceSize              indexOffset = 0;
        VkIndexType               indexType   = VK_INDEX_TYPE_UINT32;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;

} // namespace sky::vk
