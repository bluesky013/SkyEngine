//
// Created by Zach Lee on 2022/7/31.
//

#pragma once

#include <vector>
#include <vulkan/Buffer.h>
#include <vulkan/VertexInput.h>

namespace sky::drv {

    class VertexAssembly {
    public:
        VertexAssembly() = default;
        ~VertexAssembly() = default;

        void SetVertexInput(VertexInputPtr input);

        void AddVertexBuffer(const BufferView& view);

        void SetIndexBuffer(const BufferView& view);

        void SetIndexType(VkIndexType);

        void Finalize();

        void OnBind(VkCommandBuffer);

    private:
        VertexInputPtr vertexInput;
        std::vector<BufferView> vertexBuffers;
        BufferView indexBuffer;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;

        std::vector<VkBuffer> cmdVbs;
        std::vector<VkDeviceSize> cmdOffsets;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;

}
