//
// Created by blues on 2024/9/12.
//

#include <render/resource/TextureAtlas.h>

namespace sky {

    Result<TextureAtlasAllocator::Page> TextureLinearAllocator::Allocate(uint32_t w, uint32_t h)
    {
        if (currentX + w <= width && currentY + h <= height) {
            Page ret = {currentX, currentY, w, h};

            currentX += w;
            rowHeight = std::max(rowHeight, height);

            return {true, ret};
        }

        uint32_t nextY = currentY + rowHeight;
        if (w <= width && nextY + h <= height) {
            Page ret = {0, nextY, w, h};

            currentX = w;
            currentY = nextY;

            rowHeight = h;

            return {true, ret};
        }

        return {false, {}};
    }

    bool Texture2DAtlas::Init(rhi::PixelFormat format, uint32_t width, uint32_t height)
    {
        allocator = std::make_unique<TextureLinearAllocator>(width, height);
        return Texture2D::Init(format, width, height, 1);
    }

    Result<TextureAtlasAllocator::Page> Texture2DAtlas::Allocate(uint32_t w, uint32_t h)
    {
        return allocator ? allocator->Allocate(w, h) : Result<TextureAtlasAllocator::Page>{false, {}};
    }

    rhi::TransferTaskHandle Texture2DAtlas::Upload(const Page &page, rhi::Queue &queue, std::vector<uint8_t> &&data)
    {
        rhi::ImageUploadRequest request = {};
        request.offset   = 0;
        request.size     = data.size();
        request.source   = new rhi::RawBufferStream(std::move(data));
        request.mipLevel = 0;
        request.layer    = 0;
        request.imageOffset = {static_cast<int32_t>(page.x), static_cast<int32_t>(page.y), 0};
        request.imageExtent = {page.w, page.h, 1};
        return queue.UploadImage(image, request);
    }

} // namespace sky