//
// Created by Zach Lee on 2022/10/14.
//

#include <vulkan/BufferView.h>
#include <vulkan/Device.h>
#include <vulkan/Conversion.h>
#include <core/logger/Logger.h>

static const char *TAG = "Vulkan";

namespace sky::vk {

    BufferView::BufferView(Device &dev) : DevObject(dev)
    {
    }

    bool BufferView::Init(const rhi::BufferViewDesc &des)
    {
        viewDesc = des;
        return true;
    }

    std::shared_ptr<rhi::BufferView> BufferView::CreateView(const rhi::BufferViewDesc &desc) const
    {
        return source->CreateView(desc);
    }
}
