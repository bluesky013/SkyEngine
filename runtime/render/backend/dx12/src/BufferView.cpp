//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/BufferView.h>

namespace sky::dx {

    BufferView::BufferView(Device &dev) : DevObject(dev)
    {
    }

    BufferView::~BufferView() = default;

    std::shared_ptr<rhi::BufferView> BufferView::CreateView(const rhi::BufferViewDesc &desc) const
    {
        return source->CreateView(desc);
    }
}