//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/ImageView.h>

namespace sky::mtl {
    bool ImageView::Init(const rhi::ImageViewDesc &desc)
    {
        viewDesc = desc;
        return true;
    }

    std::shared_ptr<rhi::ImageView> ImageView::CreateView(const rhi::ImageViewDesc &desc) const
    {
        return source->CreateView(desc);
    }
}
