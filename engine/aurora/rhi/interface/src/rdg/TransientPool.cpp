//
// Aurora RDG transient resource pool (object pool implementation).
//

#include "TransientPool.h"

#include <aurora/rhi/Device.h>

#include <utility>

namespace sky::aurora {

    namespace {

        bool ImageDescEqual(const Image::Descriptor &a, const Image::Descriptor &b)
        {
            return a.imageType == b.imageType &&
                   a.format == b.format &&
                   a.extent.width == b.extent.width &&
                   a.extent.height == b.extent.height &&
                   a.extent.depth == b.extent.depth &&
                   a.mipLevels == b.mipLevels &&
                   a.arrayLayers == b.arrayLayers &&
                   a.samples == b.samples &&
                   a.usage == b.usage;
        }

        bool BufferDescEqual(const Buffer::Descriptor &a, const Buffer::Descriptor &b)
        {
            return a.size == b.size && a.usage == b.usage;
        }

    } // namespace

    ObjectPool::ObjectPool(Device *device) : mDevice(device) {}

    ObjectPool::~ObjectPool() = default;

    Image *ObjectPool::AcquireImage(const Image::Descriptor &desc)
    {
        for (auto &entry : mImages) {
            if (!entry.inUse && ImageDescEqual(entry.desc, desc)) {
                entry.inUse = true;
                ++mStats.imageHits;
                return entry.image.Get();
            }
        }

        ImageEntry entry;
        entry.desc  = desc;
        entry.image = CounterPtr<Image>(mDevice->CreateImage(desc));
        entry.inUse = true;
        mImages.emplace_back(std::move(entry));
        ++mStats.imageMisses;
        return mImages.back().image.Get();
    }

    Buffer *ObjectPool::AcquireBuffer(const Buffer::Descriptor &desc)
    {
        for (auto &entry : mBuffers) {
            if (!entry.inUse && BufferDescEqual(entry.desc, desc)) {
                entry.inUse = true;
                ++mStats.bufferHits;
                return entry.buffer.Get();
            }
        }

        BufferEntry entry;
        entry.desc   = desc;
        entry.buffer = CounterPtr<Buffer>(mDevice->CreateBuffer(desc));
        entry.inUse  = true;
        mBuffers.emplace_back(std::move(entry));
        ++mStats.bufferMisses;
        return mBuffers.back().buffer.Get();
    }

    void ObjectPool::ReleaseImage(Image *image)
    {
        for (auto &entry : mImages) {
            if (entry.image.Get() == image) {
                entry.inUse = false;
                return;
            }
        }
    }

    void ObjectPool::ReleaseBuffer(Buffer *buffer)
    {
        for (auto &entry : mBuffers) {
            if (entry.buffer.Get() == buffer) {
                entry.inUse = false;
                return;
            }
        }
    }

} // namespace sky::aurora
