//
// Created by Zach Lee on 2022/7/31.
//

#include <vulkan/VertexAssembly.h>

namespace sky::drv {

    void VertexAssembly::SetVertexInput(VertexInputPtr input)
    {
        vertexInput = input;
    }

    void VertexAssembly::ResetVertexBuffer()
    {
        vertexBuffers.clear();
    }

    void VertexAssembly::AddVertexBuffer(const BufferPtr& buffer, VkDeviceSize offset)
    {
        vertexBuffers.emplace_back(buffer);
        vkBuffers.emplace_back(buffer->GetNativeHandle());
        offsets.emplace_back(offset);
    }

    void VertexAssembly::SetIndexBuffer(const BufferPtr& buffer, VkDeviceSize offset)
    {
        indexBuffer = buffer;
        indexOffset = offset;
    }

    void VertexAssembly::SetIndexType(VkIndexType type)
    {
        indexType = type;
    }

    void VertexAssembly::OnBind(VkCommandBuffer cmd)
    {
        if (!vkBuffers.empty()) {
            vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(vkBuffers.size()),vkBuffers.data(), offsets.data());
        }
        if (indexBuffer) {
            vkCmdBindIndexBuffer(cmd, indexBuffer->GetNativeHandle(), indexOffset, indexType);
        }
    }

    bool VertexAssembly::IsIndexed() const
    {
        return !!indexBuffer;
    }

}