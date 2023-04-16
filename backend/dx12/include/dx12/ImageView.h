//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/ImageView.h>
#include <dx12/DevObject.h>
#include <dx12/Image.h>

namespace sky::dx {

    class ImageView : public rhi::ImageView, public DevObject {
    public:
        ImageView(Device &dev);
        ~ImageView() override;

        std::shared_ptr<rhi::ImageView> CreateView(const rhi::ImageViewDesc &desc) const;

    private:
        friend class Image;
        ImagePtr source;
    };

}