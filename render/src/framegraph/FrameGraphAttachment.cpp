//
// Created by Zach Lee on 2022/6/2.
//

#include <render/framegraph/FrameGraphAttachment.h>

namespace sky {

    void FrameGraphImageAttachment::Compile()
    {
        if (imageView) {
            return;
        }

        source->Compile();
        auto image = source->GetImage();
        auto& imageInfo = image->GetImageInfo();

        drv::ImageView::Descriptor viewDesc = {};
        viewDesc.format = imageInfo.format;
        viewDesc.subResourceRange = range;
        viewDesc.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageView = image->CreateImageView(viewDesc);
    }

}