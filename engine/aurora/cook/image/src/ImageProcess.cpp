//
// Image cook containers and pixel helpers (see ImageProcess.h).
//

#include <aurora/cook/image/ImageProcess.h>

#include <core/platform/Platform.h>

#include <algorithm>
#include <cstring>

namespace sky::aurora::cook {

    // IEEE 754 binary16 <-> binary32, so 16-bit sources can flow through the
    // float based resample/mip stages.
    float HalfToFloat(uint16_t half)
    {
        const uint32_t sign     = static_cast<uint32_t>(half & 0x8000) << 16;
        const uint32_t exponent = (half >> 10) & 0x1F;
        const uint32_t mantissa = half & 0x3FF;

            uint32_t bits = 0;
            if (exponent == 0) {
                if (mantissa == 0) {
                    bits = sign;
                } else {
                    uint32_t e = 127 - 15 + 1;
                    uint32_t m = mantissa;
                    while ((m & 0x400) == 0) {
                        m <<= 1;
                        --e;
                    }
                    m &= 0x3FF;
                    bits = sign | (e << 23) | (m << 13);
                }
            } else if (exponent == 0x1F) {
                bits = sign | 0x7F800000 | (mantissa << 13);
            } else {
                bits = sign | ((exponent + (127 - 15)) << 23) | (mantissa << 13);
            }

            float out = 0.f;
            std::memcpy(&out, &bits, sizeof(out));
            return out;
        }

    uint16_t FloatToHalf(float value)
    {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));

        const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000);
        const int32_t  exp  = static_cast<int32_t>((bits >> 23) & 0xFF) - 127 + 15;

        if (exp <= 0) {
            return sign;
        }
        if (exp >= 0x1F) {
            return static_cast<uint16_t>(sign | 0x7C00);
        }
        return static_cast<uint16_t>(sign | (exp << 10) | ((bits >> 13) & 0x3FF));
    }

    uint32_t GetMipLevel(uint32_t width, uint32_t height)
    {
        uint32_t size  = std::max(width, height);
        uint32_t level = 0;
        while (size != 0) {
            size >>= 1;
            ++level;
        }
        return level;
    }

    uint32_t GetBytePerComp(PixelFormat fmt)
    {
        const auto &info = GetImageFormatInfo(fmt);
        if (info.isCompressed || info.components == 0) {
            return 0;
        }
        return info.blockSize / info.components;
    }

    uint32_t GetNumComp(PixelFormat fmt)
    {
        return GetImageFormatInfo(fmt).components;
    }

    PixelType GetPixelType(PixelFormat fmt)
    {
        switch (fmt) {
        case PixelFormat::R8_UINT:
        case PixelFormat::R8_UNORM:
        case PixelFormat::R8_SRGB:
        case PixelFormat::RGBA8_UNORM:
        case PixelFormat::RGBA8_SRGB:
        case PixelFormat::BGRA8_UNORM:
        case PixelFormat::BGRA8_SRGB:
            return PixelType::U8;
        case PixelFormat::R16_UNORM:
        case PixelFormat::RG16_UNORM:
        case PixelFormat::RGBA16_UNORM:
        case PixelFormat::R16_SFLOAT:
        case PixelFormat::RG16_SFLOAT:
        case PixelFormat::RGBA16_SFLOAT:
            return PixelType::HALF;
        case PixelFormat::R32_SFLOAT:
        case PixelFormat::RG32_SFLOAT:
        case PixelFormat::RGB32_SFLOAT:
        case PixelFormat::RGBA32_SFLOAT:
            return PixelType::Float;
        default:
            break;
        }
        return PixelType::U8;
    }

    void GetImageColor(PixelType type, uint32_t components, const uint8_t *src, Color &color)
    {
        for (uint32_t i = 0; i < components && i < 4; ++i) {
            switch (type) {
            case PixelType::U8:
                color.v[i] = U8ToF32(src[i]);
                break;
            case PixelType::HALF: {
                const auto *ptr = reinterpret_cast<const uint16_t *>(src);
                color.v[i] = HalfToFloat(ptr[i]);
                break;
            }
            case PixelType::Float: {
                const auto *ptr = reinterpret_cast<const float *>(src);
                color.v[i] = ptr[i];
                break;
            }
            }
        }
    }

    void SetImageColor(PixelType type, uint32_t components, uint8_t *dst, const Color &color)
    {
        for (uint32_t i = 0; i < components && i < 4; ++i) {
            switch (type) {
            case PixelType::U8:
                dst[i] = F32ToU8(color.v[i]);
                break;
            case PixelType::HALF: {
                auto *ptr = reinterpret_cast<uint16_t *>(dst);
                ptr[i] = FloatToHalf(color.v[i]);
                break;
            }
            case PixelType::Float: {
                auto *ptr = reinterpret_cast<float *>(dst);
                ptr[i] = color.v[i];
                break;
            }
            }
        }
    }

    ImageObjectPtr ImageObject::CreateFromImage(const ImageObjectPtr &image)
    {
        auto res        = std::make_shared<ImageObject>();
        res->width      = image->width;
        res->height     = image->height;
        res->depth      = image->depth;
        res->components = image->components;
        res->format     = image->format;
        res->pixelSize  = image->pixelSize;
        res->type       = image->type;
        res->mips.resize(image->mips.size());
        for (size_t i = 0; i < image->mips.size(); ++i) {
            auto &mip       = res->mips[i];
            mip             = image->mips[i].CopyNoData();
            mip.rowPitch    = mip.width * res->pixelSize;
            mip.dataLength  = mip.width * mip.height * mip.depth * res->pixelSize;
            mip.data        = std::make_unique<uint8_t[]>(mip.dataLength);
        }
        return res;
    }

    ImageObjectPtr ImageObject::CreateImage2D(uint32_t width, uint32_t height, PixelFormat fmt)
    {
        auto image       = std::make_shared<ImageObject>();
        image->width     = width;
        image->height    = height;
        image->depth     = 1;
        image->components = GetNumComp(fmt);
        image->format    = fmt;
        image->pixelSize = GetBytePerComp(fmt) * image->components;
        return image;
    }

    ImageObjectPtr ImageObject::CreateImageCube(uint32_t width, uint32_t height, PixelFormat fmt)
    {
        auto image       = std::make_shared<ImageObject>();
        image->width     = width;
        image->height    = height;
        image->depth     = 6;
        image->components = GetNumComp(fmt);
        image->format    = fmt;
        image->pixelSize = GetBytePerComp(fmt) * image->components;
        return image;
    }

    void ImageObject::FillMip0()
    {
        mips.clear();
        mips.emplace_back(ImageMipData::Create(width, height, depth, pixelSize));
    }

    void ImageObject::FillMip0(const uint8_t *ptr, uint32_t size)
    {
        FillMip0();
        SKY_ASSERT(size == (width * height * depth * pixelSize));

        if (ptr != nullptr) {
            std::memcpy(mips[0].data.get(), ptr, size);
        }
    }

    CompressedImagePtr CompressedImage::CreateFromImageObject(const ImageObjectPtr &image, PixelFormat format)
    {
        auto result    = std::make_shared<CompressedImage>();
        result->width  = image->width;
        result->height = image->height;
        result->format = format;
        result->mips.resize(image->mips.size());
        for (size_t i = 0; i < image->mips.size(); ++i) {
            result->mips[i] = image->mips[i].CopyNoData();
        }
        return result;
    }

} // namespace sky::aurora::cook
