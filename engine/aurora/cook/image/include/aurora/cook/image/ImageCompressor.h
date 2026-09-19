//
// Block compression: BC7 through ispc_texcomp (runtime module) and ASTC through
// the astc-encoder static library. Compresses one mip at a time from an
// uncompressed RGBA8 source into a CompressedImage.
//

#pragma once

#include <aurora/cook/image/ImageBuildConfig.h>
#include <aurora/cook/image/ImageProcess.h>

namespace sky::aurora::cook {

    class ImageCompressor : public ImageProcess {
    public:
        struct Payload {
            ImageObjectPtr     image;
            CompressedImagePtr compressed;
            ImageBuildConfig   config;
            uint32_t           mip      = 0;
            bool               hasAlpha = false;
        };

        explicit ImageCompressor(const Payload &pd)
            : payload(pd)
        {
        }
        ~ImageCompressor() override = default;

        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::aurora::cook
