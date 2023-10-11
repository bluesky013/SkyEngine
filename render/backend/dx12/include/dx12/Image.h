//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/Image.h>
#include <dx12/DevObject.h>

namespace sky::dx {

    class Image : public rhi::Image, public DevObject {
    public:
        explicit Image(Device &dev);
        ~Image() override = default;

    private:
        bool Init(const Descriptor &);

        ComPtr<ID3D12Resource> resource;
    };
    using ImagePtr = std::shared_ptr<Image>;

} // namespace sky::dx