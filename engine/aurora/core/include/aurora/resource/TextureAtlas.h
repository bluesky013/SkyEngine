//
// TextureAtlas: a 2D texture plus an allocator-driven packer. Ports the old
// engine/render TextureAtlasAllocator / TextureLinearAllocator / Texture2DAtlas
// design onto the aurora resource layer.
//

#pragma once

#include <aurora/resource/Texture.h>
#include <core/template/Result.h>

#include <algorithm>
#include <memory>

namespace sky::aurora {

    // Atlas packer interface: allocate a texel-space sub-rectangle from a fixed
    // atlas of width x height. `Allocate` returns {false, {}} when full.
    class TextureAtlasAllocator {
    public:
        TextureAtlasAllocator(uint32_t width, uint32_t height)
            : width(width)
            , height(height)
        {
        }
        virtual ~TextureAtlasAllocator() = default;

        struct Page {
            uint32_t x;
            uint32_t y;
            uint32_t w;
            uint32_t h;
        };

        virtual Result<Page> Allocate(uint32_t w, uint32_t h) = 0;

    protected:
        uint32_t width;
        uint32_t height;
    };

    // Left-to-right, top-to-bottom linear packer; never frees.
    class TextureLinearAllocator : public TextureAtlasAllocator {
    public:
        TextureLinearAllocator(uint32_t w, uint32_t h)
            : TextureAtlasAllocator(w, h)
        {
        }

        Result<Page> Allocate(uint32_t w, uint32_t h) override
        {
            if (w == 0 || h == 0 || w > width || h > height) {
                return {false, {}};
            }
            if (currentX + w > width) {
                currentX  = 0;
                currentY += rowHeight;
                rowHeight = 0;
            }
            if (currentY + h > height) {
                return {false, {}};
            }

            Page page{currentX, currentY, w, h};
            currentX += w;
            rowHeight = std::max(rowHeight, h);
            return {true, page};
        }

    private:
        uint32_t currentX  = 0;
        uint32_t currentY  = 0;
        uint32_t rowHeight = 0;
    };

    // A 2D texture plus a packer: allocate sub-regions and upload into them.
    class TextureAtlas : public Texture2D {
    public:
        using Page = TextureAtlasAllocator::Page;

        TextureAtlas() = default;
        explicit TextureAtlas(const Name &inName) : Texture2D(inName) {}

        bool Init(Device *dev, PixelFormat format, Extent2D extent, uint32_t mipLevels = 1)
        {
            allocator = std::make_unique<TextureLinearAllocator>(extent.width, extent.height);
            return Texture2D::Init(dev, format, extent, mipLevels);
        }

        void SetAllocator(std::unique_ptr<TextureAtlasAllocator> inAllocator)
        {
            if (inAllocator != nullptr) {
                allocator = std::move(inAllocator);
            }
        }

        Result<Page> Allocate(uint32_t w, uint32_t h)
        {
            if (allocator == nullptr) {
                return {false, {}};
            }
            return allocator->Allocate(w, h);
        }

        // Upload tightly-packed data into a previously allocated page.
        bool Upload(const Page &page, const void *data, uint64_t size)
        {
            ImageUploadRequest request;
            request.source      = CounterPtr<IUploadStream>(new RawBufferStream(data, size));
            request.size        = size;
            request.mipLevel    = 0;
            request.layer       = 0;
            request.imageOffset = {static_cast<int32_t>(page.x), static_cast<int32_t>(page.y), 0};
            request.imageExtent = {page.w, page.h, 1};
            return UploadImage({request});
        }

    private:
        std::unique_ptr<TextureAtlasAllocator> allocator;
    };

} // namespace sky::aurora
