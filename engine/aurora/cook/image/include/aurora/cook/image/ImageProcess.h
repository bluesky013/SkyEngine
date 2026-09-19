//
// Image cook containers and pixel helpers, ported from the legacy render
// builder image pipeline to aurora formats. Intermediate images are always
// uncompressed (U8 / HALF / float); compression happens in a later stage.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <core/math/Color.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace sky::aurora::cook {

    enum class PixelType : uint32_t {
        U8,
        HALF,
        Float,
    };

    enum class MipGenType : uint32_t {
        Box,
        Kaiser,
        Lanczos3
    };

    enum class Quality : uint32_t {
        ULTRA_FAST,
        VERY_FAST,
        FAST,
        BASIC,
        SLOW
    };

    struct ImageMipData {
        uint32_t width      = 0;
        uint32_t height     = 0;
        uint32_t depth      = 0;
        uint32_t rowPitch   = 0;
        uint32_t dataLength = 0;
        std::unique_ptr<uint8_t[]> data;

        static ImageMipData Create(uint32_t width, uint32_t height, uint32_t depth, uint32_t pixelSize)
        {
            const uint32_t dataLength = width * height * depth * pixelSize;
            return ImageMipData{
                width, height, depth, width * pixelSize, dataLength, std::make_unique<uint8_t[]>(dataLength)
            };
        }

        ImageMipData CopyNoData() const
        {
            return ImageMipData{width, height, depth, 0, 0, nullptr};
        }
    };

    uint32_t  GetMipLevel(uint32_t width, uint32_t height);
    uint32_t  GetBytePerComp(PixelFormat fmt);
    uint32_t  GetNumComp(PixelFormat fmt);
    PixelType GetPixelType(PixelFormat fmt);

    // IEEE 754 binary16 <-> binary32; shared by the pixel helpers and the
    // resampling kernel so 16-bit sources filter correctly.
    float    HalfToFloat(uint16_t half);
    uint16_t FloatToHalf(float value);

    struct ImageObject;
    using ImageObjectPtr = std::shared_ptr<ImageObject>;
    struct ImageObject {
        uint32_t width      = 0;
        uint32_t height     = 0;
        uint32_t depth      = 0;
        uint32_t pixelSize  = 0;
        uint32_t components = 0;

        ImageType   type   = ImageType::IMAGE_2D;
        PixelFormat format = PixelFormat::UNDEFINED;

        std::vector<ImageMipData> mips;

        static ImageObjectPtr CreateFromImage(const ImageObjectPtr &image);
        static ImageObjectPtr CreateImage2D(uint32_t width, uint32_t height, PixelFormat fmt);
        static ImageObjectPtr CreateImageCube(uint32_t width, uint32_t height, PixelFormat fmt);

        void FillMip0();
        void FillMip0(const uint8_t *ptr, uint32_t size);
    };

    struct CompressedImage;
    using CompressedImagePtr = std::shared_ptr<CompressedImage>;
    struct CompressedImage {
        uint32_t    width  = 0;
        uint32_t    height = 0;
        PixelFormat format = PixelFormat::UNDEFINED;

        std::vector<ImageMipData> mips;

        static CompressedImagePtr CreateFromImageObject(const ImageObjectPtr &image, PixelFormat format);
    };

    class ImageProcess {
    public:
        ImageProcess()          = default;
        virtual ~ImageProcess() = default;

        virtual void DoWork() = 0;
    };

    void GetImageColor(PixelType type, uint32_t components, const uint8_t *src, Color &color);
    void SetImageColor(PixelType type, uint32_t components, uint8_t *dst, const Color &color);

} // namespace sky::aurora::cook
