//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include "vk_mem_alloc.h"
#include "rhi/Buffer.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::vk {

    class Device;

    class Buffer : public rhi::Buffer, public DevObject, public std::enable_shared_from_this<Buffer> {
    public:
        ~Buffer() override;

        rhi::BufferViewPtr CreateView(const rhi::BufferViewDesc &desc) override;

        uint64_t GetSize() const;
        bool IsTransient() const;
        uint8_t *Map() override;
        void UnMap() override;
        void BindMemory(VmaAllocation allocation);
        void ReleaseMemory();

        VkBuffer GetNativeHandle() const;

    private:
        friend class Device;
        friend class BufferView;

        explicit Buffer(Device &);

        bool Init(const Descriptor &);

        VkBuffer           buffer;
        VmaAllocation      allocation;
        VkBufferCreateInfo bufferInfo;
        uint8_t*           mappedPtr   = nullptr;
    };

    using BufferPtr = std::shared_ptr<Buffer>;

} // namespace sky::vk
