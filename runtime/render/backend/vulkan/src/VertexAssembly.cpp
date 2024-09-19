//
// Created by Zach Lee on 2022/7/31.
//

#include <vulkan/VertexAssembly.h>
#include <vulkan/Conversion.h>

#include <utility>

namespace sky::vk {

    void VertexAssembly::SetVertexInput(VertexInputPtr input)
    {
        vertexInput = std::move(input);
    }

    void VertexAssembly::ResetVertexBuffer()
    {
        vertexBuffers.clear();
        vkBuffers.clear();
        offsets.clear();
    }

    void VertexAssembly::AddVertexBuffer(const rhi::BufferView &buffer)
    {
        vertexBuffers.emplace_back(buffer);
        vkBuffers.emplace_back(std::static_pointer_cast<Buffer>(buffer.buffer)->GetNativeHandle());
        offsets.emplace_back(buffer.offset);
    }

    void VertexAssembly::SetIndexBuffer(const rhi::BufferView &buffer)
    {
        indexBuffer = buffer;
        indexOffset = buffer.offset;
    }

    void VertexAssembly::SetIndexType(VkIndexType type)
    {
        indexType = type;
    }

    void VertexAssembly::OnBind(VkCommandBuffer cmd) const
    {
        if (!vkBuffers.empty()) {
            vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data(), offsets.data());
        }
        if (indexBuffer.buffer) {
            vkCmdBindIndexBuffer(cmd, std::static_pointer_cast<Buffer>(indexBuffer.buffer)->GetNativeHandle(), indexOffset, indexType);
        }
    }

    bool VertexAssembly::IsIndexed() const
    {
        return static_cast<bool>(indexBuffer.buffer);
    }

    bool VertexAssembly::Init(const Descriptor &desc)
    {
        ResetVertexBuffer();
        SetVertexInput(std::static_pointer_cast<VertexInput>(desc.vertexInput));
        for (const auto &vb : desc.vertexBuffers) {
            AddVertexBuffer(vb);
        }
        SetIndexBuffer(desc.indexBuffer);
        SetIndexType(FromRHI(desc.indexType));
        return true;
    }

} // namespace sky::vk
