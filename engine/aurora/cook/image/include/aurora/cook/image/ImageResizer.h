//
// Downscale-to-limit resize (aspect preserving), ported from the legacy render
// builder. Only mip 0 is resampled; any existing chain is discarded.
//

#pragma once

#include <aurora/cook/image/ImageProcess.h>

namespace sky::aurora::cook {

    class ImageResizer : public ImageProcess {
    public:
        struct Payload {
            ImageObjectPtr image;
            uint32_t       maxWidth  = 0xFFFFFFFF;
            uint32_t       maxHeight = 0xFFFFFFFF;
            MipGenType     filterType = MipGenType::Kaiser;
        };

        explicit ImageResizer(const Payload &pd)
            : payload(pd)
        {
        }
        ~ImageResizer() override = default;

        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::aurora::cook
