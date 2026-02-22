//
// Created by blues on 2025/1/30.
//

#pragma once

#include <builder/render/image/ImageProcess.h>

namespace sky::builder {

    class ImageConverter : public ImageProcess {
    public:
        struct Payload {
            ImageObjectPtr src;
            ImageObjectPtr dst;

            float gamma = 2.2f;
        };

        explicit ImageConverter(const Payload& pd) : payload(pd) {}
        ~ImageConverter() override = default;

        void DoWork() override;

    private:
        Payload payload;
    };

} // sky::builder
