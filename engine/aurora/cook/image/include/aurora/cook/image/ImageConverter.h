//
// Per-pixel color transform (gamma) between two same-sized images, ported from
// the legacy render builder.
//

#pragma once

#include <aurora/cook/image/ImageProcess.h>

namespace sky::aurora::cook {

    class ImageConverter : public ImageProcess {
    public:
        struct Payload {
            ImageObjectPtr src;
            ImageObjectPtr dst;

            float gamma = 2.2f;
        };

        explicit ImageConverter(const Payload &pd)
            : payload(pd)
        {
        }
        ~ImageConverter() override = default;

        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::aurora::cook
