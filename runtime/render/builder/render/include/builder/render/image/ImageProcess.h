//
// Created by blues on 2025/1/29.
//

#pragma once

#include <cstdint>
#include <core/math/Color.h>
#include <core/archive/IArchive.h>
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
        HALF,
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
    uint32_t GetBytePerComp(rhi::PixelFormat fmt);
    uint32_t GetNumComp(rhi::PixelFormat fmt);
    PixelType GetPixelType(rhi::PixelFormat type);

    struct ImageObject;
    using ImageObjectPtr = std::shared_ptr<ImageObject>;
    struct ImageObject {
        bool Load(IInputArchive &archive);
        bool Save(IOutputArchive &archive) const;

        static constexpr uint32_t VERSION = 1;

        uint32_t width;
        uint32_t height;
        uint32_t depth;

        uint32_t pixelSize;
        uint32_t components;

        rhi::ImageType type = rhi::ImageType::IMAGE_2D;
        rhi::PixelFormat format = rhi::PixelFormat::UNDEFINED;

        std::vector<ImageMipData> mips;

        static ImageObjectPtr CreateFromImage(const ImageObjectPtr &image)
        {
            auto res = std::make_shared<ImageObject>();
            res->width = image->width;
            res->height = image->height;
            res->depth = image->depth;
            res->components = image->components;
            res->format = image->format;
            res->pixelSize = image->pixelSize;
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

        static ImageObjectPtr CreateImage2D(uint32_t width, uint32_t height, rhi::PixelFormat fmt)
        {
            auto image = std::make_shared<ImageObject>();
            image->width = width;
            image->height = height;
            image->depth = 1;
            image->components = GetNumComp(fmt);
            image->format = fmt;
            image->pixelSize = GetBytePerComp(fmt) * image->components;

            return image;
        }

        static ImageObjectPtr CreateImageCube(uint32_t width, uint32_t height, rhi::PixelFormat fmt)
        {
            auto image = std::make_shared<ImageObject>();
            image->width = width;
            image->height = height;
            image->depth = 6;
            image->components = GetNumComp(fmt);
            image->format = fmt;
            image->pixelSize = GetBytePerComp(fmt) * image->components;

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
        bool Load(IInputArchive &archive);
        bool Save(IOutputArchive &archive) const;

        uint32_t width;
        uint32_t height;
        rhi::PixelFormat format;

        std::vector<ImageMipData> mips;

        static CompressedImagePtr CreateFromImageObject(const ImageObjectPtr& image, rhi::PixelFormat format);
    };


    class ImageProcess {
    public:
        ImageProcess() = default;
        virtual ~ImageProcess() = default;

        virtual void DoWork() = 0;
    };

    void GetImageColor(PixelType type, uint32_t components, const uint8_t *src, Color &color);

    void SetImageColor(PixelType type, uint32_t components, uint8_t *dst, const Color &color);
} // namespace sky::builder
