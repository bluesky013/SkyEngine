//
// Created by Zach Lee on 2022/7/31.
//

#include <vulkan/VertexAssembly.h>

namespace sky::drv {

    void VertexAssembly::SetVertexInput(VertexInputPtr input)
    {
        vertexInput = input;
    }

    void VertexAssembly::AddVertexBuffer(const BufferView& view)
    {
        vertexBuffers.emplace_back(view);
    }

    void VertexAssembly::SetIndexBuffer(const BufferView& view)
    {
        indexBuffer = view;
    }

    void VertexAssembly::SetIndexType(VkIndexType type)
    {
        indexType = type;
    }

    void VertexAssembly::Finalize()
    {
        for (auto& vb : vertexBuffers) {
            cmdVbs.emplace_back(vb.buffer->GetNativeHandle());
            cmdOffsets.emplace_back(vb.offset);
        }
    }

    void VertexAssembly::OnBind(VkCommandBuffer cmd)
    {
        if (!cmdVbs.empty()) {
            vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(cmdVbs.size()),cmdVbs.data(), cmdOffsets.data());
        }
        if (!indexBuffer.buffer) {
            vkCmdBindIndexBuffer(cmd, indexBuffer.buffer->GetNativeHandle(), indexBuffer.offset, indexType);
        }
    }

}