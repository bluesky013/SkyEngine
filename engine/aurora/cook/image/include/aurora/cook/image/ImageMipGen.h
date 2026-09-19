//
// Mip chain generation (Box / Kaiser), ported from the legacy render builder.
//

#pragma once

#include <aurora/cook/image/ImageProcess.h>

namespace sky::aurora::cook {

    class ImageMipGen : public ImageProcess {
    public:
        struct Payload {
            ImageObjectPtr image;
            MipGenType     type = MipGenType::Box;
        };

        explicit ImageMipGen(const Payload &pd)
            : payload(pd)
        {
        }
        ~ImageMipGen() override = default;

        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::aurora::cook
