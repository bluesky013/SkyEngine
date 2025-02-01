//
// Created by blues on 2025/1/29.
//

#pragma once

#include <cstdint>

#include <render/adaptor/assets/ImageAsset.h>
#include <rhi/Decode.h>

namespace sky::builder {

    struct ImageBuildConfig {
        bool compress = true;
        bool generateMip = true;
        bool isNormal = false;
        bool isLinear = true;
    };

    struct ImageBuildGlobalConfig {
        PlatformType platform = PlatformType::Default;
        bool compress = true;
        bool generateMip = true;
        uint32_t maxWidth = 0xFFFFFFFF;
        uint32_t maxHeight = 0xFFFFFFFF;
        rhi::PixelFormat alphaFormat = rhi::PixelFormat::BC7_UNORM_BLOCK;
        rhi::PixelFormat opaqueFormat = rhi::PixelFormat::BC7_UNORM_BLOCK;
        rhi::PixelFormat hdrFormat = rhi::PixelFormat::BC6H_SFLOAT_BLOCK;
    };

    enum class PixelType : uint32_t {
        U8,
        Float,
    };

    enum class Quality : uint32_t {
        ULTRA_FAST,
        VERY_FAST,
        FAST,
        BASIC,
        SLOW
    };

    enum class MipGenType : uint32_t
    {
        Box,
        Triangle,
        Kaiser
    };

    struct CompressOption {
        Quality quality = Quality::SLOW;
        rhi::PixelFormat targetFormat = rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK;
        bool hasAlpha = false;
    };

    struct ImageMipData {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t rowPitch;
        uint32_t dataLength;
        std::unique_ptr<uint8_t []> data;

        static ImageMipData Create(uint32_t width, uint32_t height, uint32_t depth, uint32_t pixelSize)
        {
            uint32_t dataLength = width * height * depth * pixelSize;
            uint32_t rowPitch = width * pixelSize;
            return ImageMipData{
                width, height, depth, rowPitch, dataLength,
                std::make_unique<uint8_t []>(dataLength)
            };
        }

        ImageMipData CopyNoData() const
        {
            return ImageMipData{width, height, depth, 0, 0, nullptr};
        }
    };

    // util functions
    uint32_t GetMipLevel(uint32_t width, uint32_t height);
    uint32_t GetBytePerComp(PixelType type);

    struct ImageObject;
    using ImageObjectPtr = std::shared_ptr<ImageObject>;
    struct ImageObject {
        uint32_t width;
        uint32_t height;
        uint32_t depth;

        uint32_t pixelSize;
        PixelType pixelType;
        uint32_t components;

        rhi::ImageType type = rhi::ImageType::IMAGE_2D;

        std::vector<ImageMipData> mips;

        static ImageObjectPtr CreateFromImage(const ImageObjectPtr &image, PixelType pt, uint32_t comp)
        {
            auto res = std::make_shared<ImageObject>();
            res->width = image->width;
            res->height = image->height;
            res->depth = image->depth;
            res->pixelType = pt;
            res->components = comp;
            res->pixelSize = GetBytePerComp(pt) * comp;
            res->mips.resize(image->mips.size());
            for (uint32_t i = 0; i < image->mips.size(); ++i) {
                auto &mip = res->mips[i];
                mip = image->mips[i].CopyNoData();
                mip.rowPitch = mip.width * res->pixelSize;
                mip.dataLength = mip.width * mip.height * mip.depth * res->pixelSize;
                mip.data = std::make_unique<uint8_t[]>(mip.dataLength);
            }
            return res;
        }

        static ImageObjectPtr CreateImage2D(uint32_t width, uint32_t height, PixelType pt, uint32_t comp)
        {
            auto image = std::make_shared<ImageObject>();
            image->width = width;
            image->height = height;
            image->depth = 1;
            image->pixelType = pt;
            image->components = comp;
            image->pixelSize = GetBytePerComp(pt) * comp;

            return image;
        }

        void FillMip0()
        {
            mips.clear();
            mips.emplace_back(ImageMipData::Create(width, height, depth, pixelSize));
        }

        void FillMip0(const uint8_t *ptr, uint32_t size)
        {
            FillMip0();
            SKY_ASSERT(size == (width * height * depth * pixelSize));

            if (ptr != nullptr) {
                memcpy(mips[0].data.get(), ptr, size);
            }
        }
    };

    struct CompressedImage;
    using CompressedImagePtr = std::shared_ptr<CompressedImage>;
    struct CompressedImage {
        uint32_t width;
        uint32_t height;
        rhi::PixelFormat format;

        std::vector<ImageMipData> mips;

        static CompressedImagePtr CreateFromImageObject(const ImageObjectPtr& image, rhi::PixelFormat format)
        {
            auto result = std::make_shared<CompressedImage>();
            result->width = image->width;
            result->height = image->height;
            result->format = format;

            result->mips.resize(image->mips.size());
            for (auto i = 0; i < image->mips.size(); ++i) {
                const auto &src = image->mips[i];
                auto &dst = result->mips[i];
                dst = src.CopyNoData();
            }
            return result;
        }
    };


    class ImageProcess {
    public:
        ImageProcess() = default;
        virtual ~ImageProcess() = default;

        virtual void DoWork() = 0;
    };
} // namespace sky::builder
