//
// Created by Zach Lee on 2022/10/14.
//

#include <vulkan/BufferView.h>
#include <vulkan/Device.h>
#include <core/logger/Logger.h>

static const char *TAG = "Driver";

namespace sky::drv {

    BufferView::BufferView(Device &dev) : DevObject(dev), source{}, view{VK_NULL_HANDLE}, viewInfo{}
    {
    }

    BufferView::~BufferView()
    {
        if (view != VK_NULL_HANDLE) {
            vkDestroyBufferView(device.GetNativeHandle(), view, VKL_ALLOC);
        }
    }

    VkBufferView BufferView::GetNativeHandle() const
    {
        return view;
    }

    const VkBufferViewCreateInfo &BufferView::GetViewInfo() const
    {
        return viewInfo;
    }

    bool BufferView::Init(const Descriptor &des)
    {
        viewInfo.sType            = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        viewInfo.buffer           = source->GetNativeHandle();
        viewInfo.format           = des.format;
        viewInfo.offset          = des.offset;
        viewInfo.range = des.range;
        VkResult rst              = vkCreateBufferView(device.GetNativeHandle(), &viewInfo, VKL_ALLOC, &view);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create image view failed, -%d", rst);
        }
        return true;
    }

    std::shared_ptr<BufferView> BufferView::CreateBufferView(const BufferPtr &buffer, BufferView::Descriptor &des)
    {
        BufferViewPtr ptr = std::make_shared<BufferView>(buffer->device);
        ptr->source       = buffer;
        if (ptr->Init(des)) {
            return ptr;
        }
        return {};
    }
}