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

    void VertexAssembly::AddVertexBuffer(const rhi::BufferView &buffer)
    {
        vertexBuffers.emplace_back(buffer);
        vkBuffers.emplace_back(std::static_pointer_cast<Buffer>(buffer.buffer)->GetNativeHandle());
        offsets.emplace_back(buffer.offset);
    }

    void VertexAssembly::OnBind(VkCommandBuffer cmd) const
    {
        if (!vkBuffers.empty()) {
            vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data(), offsets.data());
        }
    }

    bool VertexAssembly::Init(const Descriptor &desc)
    {
        SetVertexInput(std::static_pointer_cast<VertexInput>(desc.vertexInput));
        for (const auto &vb : desc.vertexBuffers) {
            AddVertexBuffer(vb);
        }
        return true;
    }

} // namespace sky::vk
