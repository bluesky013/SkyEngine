//
// Created by Zach on 2023/1/31.
//

#include <gles/BufferView.h>
#include <gles/Device.h>

namespace sky::gles {

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
