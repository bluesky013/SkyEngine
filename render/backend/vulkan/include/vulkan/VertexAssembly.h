//
// Created by Zach Lee on 2022/7/31.
//

#pragma once

#include <vector>
#include <vulkan/Buffer.h>
#include <vulkan/BufferView.h>
#include <vulkan/VertexInput.h>
#include <vulkan/DevObject.h>
#include <rhi/VertexAssembly.h>

namespace sky::vk {
    class Device;

    class VertexAssembly : public rhi::VertexAssembly, public DevObject {
    public:
        VertexAssembly(Device &dev) : DevObject(dev) {}
        ~VertexAssembly() = default;

        void SetVertexInput(VertexInputPtr input);

        void ResetVertexBuffer();

        void AddVertexBuffer(const BufferViewPtr &buffer);

        void SetIndexBuffer(const BufferViewPtr &buffer);

        void SetIndexType(VkIndexType);

        void OnBind(VkCommandBuffer) const;

        bool IsIndexed() const;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        VertexInputPtr             vertexInput;
        std::vector<BufferViewPtr> vertexBuffers;
        std::vector<VkBuffer>      vkBuffers;
        std::vector<VkDeviceSize>  offsets;
        BufferViewPtr              indexBuffer;
        VkDeviceSize               indexOffset = 0;
        VkIndexType                indexType   = VK_INDEX_TYPE_UINT32;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;

} // namespace sky::vk
