//
// Aurora RDG transient resource pool (internal).
//

#pragma once

#include <aurora/rdg/RDGTypes.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Image.h>

#include <vector>

namespace sky::aurora {

    class Device;

    // Abstract transient resource pool. v1 ships ObjectPool (whole-object reuse);
    // a memory-heap aliasing pool is reserved for v2.
    class TransientPool {
    public:
        virtual ~TransientPool() = default;

        virtual Image  *AcquireImage(const Image::Descriptor &desc)  = 0;
        virtual Buffer *AcquireBuffer(const Buffer::Descriptor &desc) = 0;

        virtual void ReleaseImage(Image *image)   = 0;
        virtual void ReleaseBuffer(Buffer *buffer) = 0;

        virtual const TransientPoolStats &GetStats() const = 0;
    };

    // Whole-object cache keyed by the full descriptor. Lifetime-non-overlapping
    // resources share a backing object via Acquire/Release cycles.
    class ObjectPool : public TransientPool {
    public:
        explicit ObjectPool(Device *device);
        ~ObjectPool() override;

        Image  *AcquireImage(const Image::Descriptor &desc) override;
        Buffer *AcquireBuffer(const Buffer::Descriptor &desc) override;

        void ReleaseImage(Image *image) override;
        void ReleaseBuffer(Buffer *buffer) override;

        const TransientPoolStats &GetStats() const override { return mStats; }

    private:
        struct ImageEntry {
            Image::Descriptor desc;
            ImagePtr          image;
            bool              inUse = false;
        };
        struct BufferEntry {
            Buffer::Descriptor desc;
            BufferPtr          buffer;
            bool               inUse = false;
        };

        Device                  *mDevice = nullptr;
        std::vector<ImageEntry>  mImages;
        std::vector<BufferEntry> mBuffers;
        TransientPoolStats       mStats;
    };

} // namespace sky::aurora
