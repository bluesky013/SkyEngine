//
// Created by Zach Lee on 2021/12/22.
//

#include <engine/render/RenderView.h>

namespace sky {

    const RenderView::Descriptor& RenderView::GetDescriptor() const
    {
        return descriptor;
    }
}