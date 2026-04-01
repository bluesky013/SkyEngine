//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Image.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalImage : public Image {
    public:
        explicit MetalImage(MetalDevice &dev);
        ~MetalImage() override;

        bool Init(const Descriptor &desc);

        void *GetNativeHandle() const { return texture; }

    private:
        MetalDevice &device;
        void        *texture = nullptr;
    };

} // namespace sky::aurora