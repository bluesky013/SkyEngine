//
// Created by Zach Lee on 2022/7/31.
//

#include <vulkan/VertexAssembly.h>
#include <vulkan/Conversion.h>

namespace sky::vk {

    void VertexAssembly::SetVertexInput(VertexInputPtr input)
    {
        vertexInput = input;
    }

    void VertexAssembly::ResetVertexBuffer()
    {
        vertexBuffers.clear();
        vkBuffers.clear();
        offsets.clear();
    }

    void VertexAssembly::AddVertexBuffer(const BufferViewPtr &buffer)
    {
        vertexBuffers.emplace_back(buffer);
        vkBuffers.emplace_back(buffer->GetBuffer()->GetNativeHandle());
        offsets.emplace_back(buffer->GetViewDesc().offset);
    }

    void VertexAssembly::SetIndexBuffer(const BufferViewPtr &buffer)
    {
        indexBuffer = buffer;
        indexOffset = buffer ? buffer->GetViewDesc().offset : 0;
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
        if (indexBuffer) {
            vkCmdBindIndexBuffer(cmd, indexBuffer->GetBuffer()->GetNativeHandle(), indexOffset, indexType);
        }
    }

    bool VertexAssembly::IsIndexed() const
    {
        return !!indexBuffer;
    }

    bool VertexAssembly::Init(const Descriptor &desc)
    {
        ResetVertexBuffer();
        SetVertexInput(std::static_pointer_cast<VertexInput>(desc.vertexInput));
        for (auto &vb : desc.vertexBuffers) {
            AddVertexBuffer(std::static_pointer_cast<BufferView>(vb));
        }
        SetIndexBuffer(std::static_pointer_cast<BufferView>(desc.indexBuffer));
        SetIndexType(FromRHI(desc.indexType));
        return true;
    }

} // namespace sky::vk
