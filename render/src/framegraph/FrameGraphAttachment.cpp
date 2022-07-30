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
        imageView = drv::ImageView::CreateImageView(image, viewDesc);
    }

    FrameGraphImageAttachment& FrameGraphImageAttachment::SetColorOp(VkAttachmentLoadOp load, VkAttachmentStoreOp store)
    {
        loadOp = load;
        storeOp = store;
        return *this;
    }

    FrameGraphImageAttachment& FrameGraphImageAttachment::SetClearValue(VkClearValue clear)
    {
        clearValue = clear;
        return *this;
    }

    void FrameGraphBufferAttachment::Execute(drv::CommandBufferPtr commandBuffer)
    {
        
    }

    void FrameGraphBufferAttachment::Compile()
    {

    }

}