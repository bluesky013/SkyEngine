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

        void OnBind(VkCommandBuffer) const;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        void SetVertexInput(VertexInputPtr input);
        void AddVertexBuffer(const rhi::BufferView &buffer);

        VertexInputPtr               vertexInput;
        std::vector<rhi::BufferView> vertexBuffers;
        std::vector<VkBuffer>        vkBuffers;
        std::vector<VkDeviceSize>    offsets;
    };
    using VertexAssemblyPtr = std::shared_ptr<VertexAssembly>;

} // namespace sky::vk
