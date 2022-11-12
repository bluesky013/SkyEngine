//
// Created by Zach Lee on 2022/11/12.
//

#include <rhi/ImageView.h>

namespace sky::rhi {

    const ImageView::Descriptor &ImageView::GetDescriptor() const
    {
        return viewDesc;
    }

}