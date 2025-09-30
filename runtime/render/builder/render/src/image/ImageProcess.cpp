//
// Created by blues on 2025/1/29.
//

#include <builder/render/image/ImageProcess.h>

namespace sky::builder {

    uint32_t GetMipLevel(uint32_t width, uint32_t height)
    {
        uint32_t size = std::max(width, height);
        uint32_t level = 0;
        while (size != 0) {
            size >>= 1;
            ++level;
        }
        return level;
    }

    uint32_t GetBytePerComp(rhi::PixelFormat fmt)
    {
        switch (fmt) {
        case rhi::PixelFormat::R8_UNORM: return 1;
        case rhi::PixelFormat::R8_SRGB: return 1;
        case rhi::PixelFormat::RGBA8_UNORM: return 1;
        case rhi::PixelFormat::RGBA8_SRGB: return 1;
        case rhi::PixelFormat::BGRA8_UNORM: return 1;
        case rhi::PixelFormat::BGRA8_SRGB: return 1;
        case rhi::PixelFormat::RGBA16_SFLOAT: return 2;
        case rhi::PixelFormat::R16_UNORM: return 2;
        case rhi::PixelFormat::R32_SFLOAT: return 4;
        case rhi::PixelFormat::RG32_SFLOAT: return 4;
        case rhi::PixelFormat::RGB32_SFLOAT: return 4;
        case rhi::PixelFormat::RGBA32_SFLOAT: return 4;
        default:
            break;
        }
        return 0;
    }

    uint32_t GetNumComp(rhi::PixelFormat fmt)
    {
        switch (fmt) {
        case rhi::PixelFormat::R8_UNORM: return 1;
        case rhi::PixelFormat::R8_SRGB: return 1;
        case rhi::PixelFormat::RGBA8_UNORM: return 4;
        case rhi::PixelFormat::RGBA8_SRGB: return 4;
        case rhi::PixelFormat::BGRA8_UNORM: return 4;
        case rhi::PixelFormat::BGRA8_SRGB: return 4;
        case rhi::PixelFormat::RGBA16_SFLOAT: return 4;
        case rhi::PixelFormat::R16_UNORM: return 1;
        case rhi::PixelFormat::R32_SFLOAT: return 1;
        case rhi::PixelFormat::RG32_SFLOAT: return 2;
        case rhi::PixelFormat::RGB32_SFLOAT: return 3;
        case rhi::PixelFormat::RGBA32_SFLOAT: return 4;
        default:
            break;
        }
        return 0;
    }

    PixelType GetPixelType(rhi::PixelFormat fmt)
    {
        switch (fmt) {
        case rhi::PixelFormat::R8_UNORM: return PixelType::U8;
        case rhi::PixelFormat::R8_SRGB: return PixelType::U8;
        case rhi::PixelFormat::RGBA8_UNORM: return PixelType::U8;
        case rhi::PixelFormat::RGBA8_SRGB: return PixelType::U8;
        case rhi::PixelFormat::BGRA8_UNORM: return PixelType::U8;
        case rhi::PixelFormat::BGRA8_SRGB: return PixelType::U8;
        case rhi::PixelFormat::RGBA16_SFLOAT: return PixelType::HALF;
        case rhi::PixelFormat::R16_UNORM: return PixelType::HALF;
        case rhi::PixelFormat::R32_SFLOAT: return PixelType::Float;
        case rhi::PixelFormat::RG32_SFLOAT: return PixelType::Float;
        case rhi::PixelFormat::RGB32_SFLOAT: return PixelType::Float;
        case rhi::PixelFormat::RGBA32_SFLOAT: return PixelType::Float;
        default:
            break;
        }
        return PixelType::U8;
    }

    void GetImageColor(PixelType type, uint32_t components, const uint8_t *src, Color &color)
    {
        for (uint32_t i = 0; i < components; ++i) {
            switch (type) {
            case PixelType::U8:
                color.v[i] = U8ToF32(src[i]);
                break;
            case PixelType::Float:
            {
                const auto *ptr = reinterpret_cast<const float *>(src);
                color.v[i] = ptr[i];
            }
            break;
            default: SKY_ASSERT("not implement") break;
            }
        }
    }

    void SetImageColor(PixelType type, uint32_t components, uint8_t *dst, const Color &color)
    {
        for (uint32_t i = 0; i < components; ++i) {
            switch (type) {
            case PixelType::U8:
                dst[i] = F32ToU8(color.v[i]);
                break;
            case PixelType::Float:
            {
                auto *ptr = reinterpret_cast<float *>(dst);
                ptr[i] = color.v[i];
            }
            default:
                SKY_ASSERT("not implement")
                break;
            }
        }
    }

    static constexpr uint32_t IMAGE_MAGIC = MakeMagic('S', 'I', 'M', 'G');
    static constexpr uint32_t IMAGE_MAGIC_COMPRESSED = MakeMagic('C', 'I', 'M', 'G');

    bool ImageObject::Load(IInputArchive &archive)
    {
        uint32_t val = 0;
        archive.Load(val); // magic

        if (val != IMAGE_MAGIC) {
            return false;
        }

        archive.Load(val); // version
        if (val != VERSION) {
            return false;
        }

        archive.Load(width);
        archive.Load(height);
        archive.Load(depth);
        archive.Load(pixelSize);
        archive.Load(components);
        archive.Load(type);
        archive.Load(format);

        archive.Load(val); // mips
        mips.resize(val);

        for (uint32_t i = 0; i < val; ++i) {
            auto& mip = mips[i];
            archive.Load(mip.width);
            archive.Load(mip.height);
            archive.Load(mip.depth);
            archive.Load(mip.rowPitch);
            archive.Load(mip.dataLength);
        }


        for (uint32_t i = 0; i < val; ++i) {
            auto& mip = mips[i];
            mip.data = std::make_unique<uint8_t[]>(mip.dataLength);
            archive.LoadRaw(reinterpret_cast<char *>(mip.data.get()), mip.dataLength);
        }
        return true;
    }

    bool ImageObject::Save(IOutputArchive &archive) const
    {
        archive.Save(IMAGE_MAGIC);
        archive.Save(VERSION);

        archive.Save(width);
        archive.Save(height);
        archive.Save(depth);
        archive.Save(pixelSize);
        archive.Save(components);
        archive.Save(type);
        archive.Save(format);

        archive.Save(static_cast<uint32_t>(mips.size()));

        for (const auto & mip : mips) {
            archive.Save(mip.width);
            archive.Save(mip.height);
            archive.Save(mip.depth);
            archive.Save(mip.rowPitch);
            archive.Save(mip.dataLength);
        }

        for (const auto& mip : mips) {
            archive.SaveRaw(reinterpret_cast<const char *>(mip.data.get()), mip.dataLength);
        }

        return true;
    }

    bool CompressedImage::Load(IInputArchive &archive)
    {
        uint32_t val = 0;
        archive.Load(val); // magic

        if (val != IMAGE_MAGIC) {
            return false;
        }

        archive.Load(val); // version
        if (val != ImageObject::VERSION) {
            return false;
        }

        archive.Load(width);
        archive.Load(height);
        archive.Load(format);

        archive.Load(val); // mips
        mips.resize(val);

        for (uint32_t i = 0; i < val; ++i) {
            auto& mip = mips[i];
            archive.Load(mip.width);
            archive.Load(mip.height);
            archive.Load(mip.depth);
            archive.Load(mip.rowPitch);
            archive.Load(mip.dataLength);
        }

        for (uint32_t i = 0; i < val; ++i) {
            auto& mip = mips[i];
            mip.data = std::make_unique<uint8_t[]>(mip.dataLength);
            archive.LoadRaw(reinterpret_cast<char *>(mip.data.get()), mip.dataLength);
        }

        return true;
    }

    bool CompressedImage::Save(IOutputArchive &archive) const
    {
        archive.Save(IMAGE_MAGIC);
        archive.Save(ImageObject::VERSION);

        archive.Save(width);
        archive.Save(height);
        archive.Save(format);

        archive.Save(static_cast<uint32_t>(mips.size()));

        for (const auto & mip : mips) {
            archive.Save(mip.width);
            archive.Save(mip.height);
            archive.Save(mip.depth);
            archive.Save(mip.rowPitch);
            archive.Save(mip.dataLength);
        }

        for (const auto& mip : mips) {
            archive.SaveRaw(reinterpret_cast<const char *>(mip.data.get()), mip.dataLength);
        }

        return true;
    }

    CompressedImagePtr CompressedImage::CreateFromImageObject(const ImageObjectPtr& image, rhi::PixelFormat format)
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
} // namespace sky::builder