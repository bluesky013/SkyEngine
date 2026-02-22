//
// Created by blues on 2024/9/12.
//

#pragma once

#include <render/resource/Texture.h>
#include <core/template/Result.h>

namespace sky {

    class TextureAtlasAllocator {
    public:
        TextureAtlasAllocator(uint32_t w, uint32_t h) : width(w), height(h) {}
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

    // allocate for left -> right, top -> bottom, never free.
    class TextureLinearAllocator : public TextureAtlasAllocator {
    public:
        TextureLinearAllocator(uint32_t w, uint32_t h) : TextureAtlasAllocator(w, h) {}
        ~TextureLinearAllocator() override = default;

        Result<Page> Allocate(uint32_t w, uint32_t h) override;

    private:
        uint32_t currentX = 0;
        uint32_t currentY = 0;

        uint32_t rowHeight = 0;
    };

    class Texture2DAtlas : public Texture2D {
    public:
        Texture2DAtlas() = default;
        ~Texture2DAtlas() override = default;

        using Page = TextureAtlasAllocator::Page;

        bool Init(rhi::PixelFormat format, uint32_t width, uint32_t height);

        Result<TextureAtlasAllocator::Page> Allocate(uint32_t w, uint32_t h);

        void Upload(const Page &page, rhi::Queue &queue, std::vector<uint8_t> &&data);

    private:
        std::unique_ptr<TextureAtlasAllocator> allocator;
    };
    using RDTexture2DAtlasPtr = CounterPtr<Texture2DAtlas>;

} // namespace sky