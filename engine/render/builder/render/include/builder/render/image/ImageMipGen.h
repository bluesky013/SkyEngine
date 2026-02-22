//
// Created by blues on 2025/1/30.
//

#pragma once

#include <builder/render/image/ImageProcess.h>

namespace sky::builder {

    class ImageMipGen : public ImageProcess {
    public:
        struct Payload {
            ImageObjectPtr image;
            MipGenType type = MipGenType::Box;
        };

        explicit ImageMipGen(const Payload& pd) : payload(pd) {}
        ~ImageMipGen() override = default;

        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::builder
