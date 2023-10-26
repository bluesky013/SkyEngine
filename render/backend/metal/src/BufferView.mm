//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/BufferView.h>

namespace sky::mtl {

    bool BufferView::Init(const rhi::BufferViewDesc &desc)
    {
        viewDesc = desc;
        return true;
    }

    std::shared_ptr<rhi::BufferView> BufferView::CreateView(const rhi::BufferViewDesc &desc) const
    {
        return source->CreateView(desc);
    }
}
