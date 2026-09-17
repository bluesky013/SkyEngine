//
// Texture: high-level render resource for images. Wraps a lazily-created
// rhi::Image with a unified upload entry point, plus dimension-specialized
// subclasses (2D / cube / 2D array / 3D) and an allocator-driven atlas.
//

#pragma once

#include <aurora/resource/Buffer.h>
#include <aurora/resource/RenderResource.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Queue.h>
#include <core/name/Name.h>

#include <vector>

namespace sky::aurora {

    // Image-side RenderResource subclass: pure image wrapper (no view/sampler).
    // Lazy Create() delegates to Device::CreateImage; UploadImage delegates to
    // Queue::UploadImage. v1 textures are always GPU_ONLY.
    class Texture : public RenderResource {
    public:
        Texture() = default;
        explicit Texture(const Name &inName) : RenderResource(inName) {}
        ~Texture() override { WaitUploadComplete(); }

        // Raw/generic entry: caller supplies the full Image::Descriptor (imageType /
        // viewUsage / usage included). Only the v1 invariant (GPU_ONLY) is forced.
        bool Init(Device *dev, const Image::Descriptor &inDesc)
        {
            device = dev;
            desc   = inDesc;
            desc.memory = MemoryType::GPU_ONLY;
            return dev != nullptr;
        }

        // Convenience upload: treats `data` as the whole mip-0, layer-0
        // tightly-packed image. `offset` is a source byte offset. Multi-layer
        // textures (cube / 2D array) must use UploadImage for every layer.
        bool Upload(const void *data, uint64_t size, uint64_t offset = 0) override
        {
            ImageUploadRequest request;
            request.source      = CounterPtr<IUploadStream>(new RawBufferStream(data, size));
            request.offset      = offset;
            request.size        = size;
            request.mipLevel    = 0;
            request.layer       = 0;
            request.imageExtent = desc.extent;
            return UploadImage({request});
        }

        // Sub-resource upload: per-mip / per-layer / per-region, delegated to RHI.
        bool UploadImage(const std::vector<ImageUploadRequest> &requests)
        {
            if (device == nullptr) {
                return false;
            }
            if (!created) {
                Create();
            }
            if (!created) {
                return false;
            }

            auto *queue = device->GetQueue(QueueType::TRANSFER);
            if (queue == nullptr) {
                queue = device->GetQueue(QueueType::GRAPHICS);
            }
            if (queue == nullptr) {
                return false;
            }
            pendingHandle = queue->UploadImage(image.Get(), requests);
            pendingQueue  = queue;
            return true;
        }

        Image       *GetImage() const { return image.Get(); }
        const Image::Descriptor &GetDescriptor() const { return desc; }
        Extent3D     GetExtent() const { return desc.extent; }
        uint32_t     GetMipLevels() const { return desc.mipLevels; }
        uint32_t     GetArrayLayers() const { return desc.arrayLayers; }
        PixelFormat  GetFormat() const { return desc.format; }

        // Async upload completion: query or block on the pending transfer task.
        bool IsUploadComplete() const
        {
            return pendingQueue == nullptr || pendingQueue->HasComplete(pendingHandle);
        }

        void WaitUploadComplete()
        {
            if (pendingQueue != nullptr) {
                pendingQueue->Wait(pendingHandle);
                pendingQueue = nullptr;
            }
        }

    protected:
        void Create() override
        {
            if (created || device == nullptr) {
                return;
            }
#if SKY_ENABLE_RESOURCE_NAME
            desc.name = name.GetStr().data();
#endif
            image   = ImagePtr(device->CreateImage(desc));
            created = image != nullptr;
        }

        void Release() override
        {
            WaitUploadComplete();
            image   = nullptr;
            created = false;
        }

        Image::Descriptor desc;
        ImagePtr          image;
        TransferTaskHandle pendingHandle = 0;
        Queue            *pendingQueue   = nullptr;
    };

    class Texture2D : public Texture {
    public:
        Texture2D() = default;
        explicit Texture2D(const Name &inName) : Texture(inName) {}

        bool Init(Device *dev, PixelFormat format, Extent2D extent, uint32_t mipLevels = 1, SampleCount samples = SampleCount::X1)
        {
            Image::Descriptor desc;
            desc.imageType   = ImageType::IMAGE_2D;
            desc.format      = format;
            desc.extent      = {extent.width, extent.height, 1};
            desc.mipLevels   = mipLevels;
            desc.arrayLayers = 1;
            desc.samples     = samples;
            desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
            return Texture::Init(dev, desc);
        }
    };

    class TextureCube : public Texture {
    public:
        TextureCube() = default;
        explicit TextureCube(const Name &inName) : Texture(inName) {}

        bool Init(Device *dev, PixelFormat format, Extent2D extent, uint32_t mipLevels = 1)
        {
            Image::Descriptor desc;
            desc.imageType   = ImageType::IMAGE_2D;
            desc.format      = format;
            desc.extent      = {extent.width, extent.height, 1};
            desc.mipLevels   = mipLevels;
            desc.arrayLayers = 6;
            desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
            desc.viewUsage   = ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE;
            return Texture::Init(dev, desc);
        }
    };

    class Texture2DArray : public Texture {
    public:
        Texture2DArray() = default;
        explicit Texture2DArray(const Name &inName) : Texture(inName) {}

        bool Init(Device *dev, PixelFormat format, Extent2D extent, uint32_t arrayLayers, uint32_t mipLevels = 1)
        {
            Image::Descriptor desc;
            desc.imageType   = ImageType::IMAGE_2D;
            desc.format      = format;
            desc.extent      = {extent.width, extent.height, 1};
            desc.mipLevels   = mipLevels;
            desc.arrayLayers = arrayLayers;
            desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
            return Texture::Init(dev, desc);
        }
    };

    class Texture3D : public Texture {
    public:
        Texture3D() = default;
        explicit Texture3D(const Name &inName) : Texture(inName) {}

        bool Init(Device *dev, PixelFormat format, Extent3D extent, uint32_t mipLevels = 1)
        {
            Image::Descriptor desc;
            desc.imageType   = ImageType::IMAGE_3D;
            desc.format      = format;
            desc.extent      = extent;
            desc.mipLevels   = mipLevels;
            desc.arrayLayers = 1;
            desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
            return Texture::Init(dev, desc);
        }
    };

} // namespace sky::aurora
