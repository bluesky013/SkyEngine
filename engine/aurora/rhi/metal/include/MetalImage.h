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

        bool Init(const Descriptor &desc, bool forceArray = false);

        // Wrap an externally-owned MTLTexture (e.g. from a CAMetalDrawable).
        // Caller is responsible for keeping it alive until Reset() / destruction.
        void RebindBorrowed(void *nativeTexture);

        // Release any current texture reference (also drops borrowed wrappers).
        void Reset();

        // Create an owned mip/layer view of the current texture. The caller
        // owns the returned image; the base texture must outlive it.
        MetalImage *CreateView(uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount, bool arrayView = false);

        void *GetNativeHandle() const
        {
            return texture;
        }
        const Device *GetDevice() const noexcept override;
        uint64_t      GetMipLayerCount() const noexcept override
        {
            return uint64_t(mipLevels) * arrayLayers;
        }
        const Descriptor &GetDescriptor() const noexcept override
        {
            return descriptor;
        }
        PixelFormat GetPixelFormat() const
        {
            return pixelFormat;
        }

    private:
        Descriptor   descriptor;
        MetalDevice &device;
        void        *texture     = nullptr;
        bool         owned       = true;
        PixelFormat  pixelFormat = PixelFormat::UNDEFINED;
        uint32_t     mipLevels   = 1;
        uint32_t     arrayLayers = 1;
    };

} // namespace sky::aurora
