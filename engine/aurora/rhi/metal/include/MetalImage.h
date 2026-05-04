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

        // Wrap an externally-owned MTLTexture (e.g. from a CAMetalDrawable).
        // Caller is responsible for keeping it alive until Reset() / destruction.
        void RebindBorrowed(void *nativeTexture);

        // Release any current texture reference (also drops borrowed wrappers).
        void Reset();

        void *GetNativeHandle() const { return texture; }

    private:
        MetalDevice &device;
        void        *texture = nullptr;
        bool         owned   = true;
    };

} // namespace sky::aurora