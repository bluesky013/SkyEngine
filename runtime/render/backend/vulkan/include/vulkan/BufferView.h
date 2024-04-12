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
        explicit BufferView(Device &);
        ~BufferView() override = default;

        std::shared_ptr<rhi::BufferView> CreateView(const rhi::BufferViewDesc &) const override;

        const BufferPtr &GetBuffer() const { return source; }

    private:
        friend class Buffer;

        bool Init(const rhi::BufferViewDesc &);

        BufferPtr source;
    };

    using BufferViewPtr = std::shared_ptr<BufferView>;
} // namespace sky::vk
