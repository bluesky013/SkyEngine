//
// Created by Zach Lee on 2022/7/31.
//

#pragma once

#include <vector>
#include <vulkan/Buffer.h>
#include <vulkan/VertexInput.h>
#include <vulkan/DevObject.h>
#include <rhi/VertexAssembly.h>

namespace sky::vk {
    class Device;

    class VertexAssembly : public rhi::VertexAssembly, public DevObject {
    public:
        explicit VertexAssembly(Device &dev) : DevObject(dev) {}
        ~VertexAssembly() override = default;

        void SetVertexInput(VertexInputPtr input);

        void ResetVertexBuffer();

        void AddVertexBuffer(const rhi::BufferView &buffer);

        void SetIndexBuffer(const rhi::BufferView &buffer);

        void SetIndexType(VkIndexType);

        void OnBind(VkCommandBuffer) const;

        bool IsIndexed() const;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        VertexInputPtr               vertexInput;
        std::vector<rhi::BufferView> vertexBuffers;
        std::vector<VkBuffer>        vkBuffers;
        std::vector<VkDeviceSize>    offsets;
        rhi::BufferView              indexBuffer;
        VkDeviceSize                 indexOffset = 0;
        VkIndexType                  indexType   = VK_INDEX_TYPE_UINT32;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;

} // namespace sky::vk
