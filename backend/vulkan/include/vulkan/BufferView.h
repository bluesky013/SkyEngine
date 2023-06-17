//
// Created by Zach Lee on 2022/10/14.
//

#pragma once
#include <vk_mem_alloc.h>
#include <rhi/BufferView.h>
#include <vulkan/DevObject.h>
#include <vulkan/Buffer.h>

namespace sky::vk {

    class Device;
    class Image;

    class BufferView : public rhi::BufferView, public DevObject {
    public:
        BufferView(Device &);
        ~BufferView();

        struct VkDescriptor {
            VkFormat format = VK_FORMAT_UNDEFINED;
            VkDeviceSize offset = 0;
            VkDeviceSize range  = VK_WHOLE_SIZE;
        };

        std::shared_ptr<rhi::BufferView> CreateView(const rhi::BufferViewDesc &) const override;
        static std::shared_ptr<BufferView> CreateBufferView(const BufferPtr &buffer, const BufferView::VkDescriptor &des);

        const BufferPtr &GetBuffer() const { return source; }
        VkBufferView GetNativeHandle() const { return view; }

    private:
        friend class Buffer;
        friend class SwapChain;

        bool Init(const rhi::BufferViewDesc &);
        bool Init(const VkDescriptor &);

        BufferPtr              source;
        VkBufferView           view;
        VkBufferViewCreateInfo viewInfo;
    };

    using BufferViewPtr = std::shared_ptr<BufferView>;
} // namespace sky::vk
