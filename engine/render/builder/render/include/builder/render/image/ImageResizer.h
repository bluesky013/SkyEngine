//
// Created by blues on 2025/2/1.
//

#pragma once

#include <builder/render/image/ImageProcess.h>

namespace sky::builder {

    class ImageResizer : public ImageProcess {
    public:
        struct Payload {
            ImageObjectPtr image;
            uint32_t maxWidth  = 0xFFFFFFFF;
            uint32_t maxHeight = 0xFFFFFFFF;
            MipGenType filterType = MipGenType::Kaiser;
        };

        explicit ImageResizer(const Payload &pd) : payload(pd) {}
        ~ImageResizer() override = default;

        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::builder
