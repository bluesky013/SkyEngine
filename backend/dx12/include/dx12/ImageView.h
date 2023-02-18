//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/ImageView.h>
#include <dx12/DevObject.h>

namespace sky::dx {

    class ImageView : public rhi::ImageView, public DevObject {
    public:
        ImageView(Device &dev);
        ~ImageView() override;
    };

}