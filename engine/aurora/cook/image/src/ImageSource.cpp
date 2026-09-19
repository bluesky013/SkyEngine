//
// Source decoding: stb (png/jpg/jpeg/hdr) and a minimal KTX1/KTX2 reader.
//

#include <aurora/cook/image/ImageSource.h>

#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

#include <cstring>
#include <stb_image.h>

static const char *TAG = "AuroraImageSource";

namespace sky::aurora::cook {

    namespace {

        constexpr uint8_t KTX1_ID[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
        constexpr uint8_t KTX2_ID[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

        uint32_t ReadU32(const std::vector<uint8_t> &bytes, size_t offset)
        {
            uint32_t value = 0;
            if (offset + 4 <= bytes.size()) {
                std::memcpy(&value, bytes.data() + offset, 4);
            }
            return value;
        }

        uint64_t ReadU64(const std::vector<uint8_t> &bytes, size_t offset)
        {
            uint64_t value = 0;
            if (offset + 8 <= bytes.size()) {
                std::memcpy(&value, bytes.data() + offset, 8);
            }
            return value;
        }

        size_t Align4(size_t value)
        {
            return (value + 3) & ~static_cast<size_t>(3);
        }

        uint32_t MipDim(uint32_t base, uint32_t level)
        {
            return base > level ? (base >> level) : 1u;
        }

        PixelFormat MapVkFormat(uint32_t vk)
        {
            switch (vk) {
            case 37: return PixelFormat::RGBA8_UNORM;
            case 43: return PixelFormat::RGBA8_SRGB;
            case 131: return PixelFormat::BC1_RGB_UNORM_BLOCK;
            case 132: return PixelFormat::BC1_RGB_SRGB_BLOCK;
            case 133: return PixelFormat::BC1_RGBA_UNORM_BLOCK;
            case 134: return PixelFormat::BC1_RGBA_SRGB_BLOCK;
            case 135: return PixelFormat::BC2_UNORM_BLOCK;
            case 136: return PixelFormat::BC2_SRGB_BLOCK;
            case 137: return PixelFormat::BC3_UNORM_BLOCK;
            case 138: return PixelFormat::BC3_SRGB_BLOCK;
            case 139: return PixelFormat::BC4_UNORM_BLOCK;
            case 140: return PixelFormat::BC4_SNORM_BLOCK;
            case 141: return PixelFormat::BC5_UNORM_BLOCK;
            case 142: return PixelFormat::BC5_SNORM_BLOCK;
            case 143: return PixelFormat::BC6H_UFLOAT_BLOCK;
            case 144: return PixelFormat::BC6H_SFLOAT_BLOCK;
            case 145: return PixelFormat::BC7_UNORM_BLOCK;
            case 146: return PixelFormat::BC7_SRGB_BLOCK;
            case 157: return PixelFormat::ASTC_4x4_UNORM_BLOCK;
            case 158: return PixelFormat::ASTC_4x4_SRGB_BLOCK;
            case 171: return PixelFormat::ASTC_8x8_UNORM_BLOCK;
            case 172: return PixelFormat::ASTC_8x8_SRGB_BLOCK;
            case 179: return PixelFormat::ASTC_10x10_UNORM_BLOCK;
            case 180: return PixelFormat::ASTC_10x10_SRGB_BLOCK;
            case 183: return PixelFormat::ASTC_12x12_UNORM_BLOCK;
            case 184: return PixelFormat::ASTC_12x12_SRGB_BLOCK;
            default: break;
            }
            return PixelFormat::UNDEFINED;
        }

        PixelFormat MapGlInternalFormat(uint32_t gl)
        {
            switch (gl) {
            case 0x8058: return PixelFormat::RGBA8_UNORM;        // GL_RGBA8
            case 0x8C43: return PixelFormat::RGBA8_SRGB;         // GL_SRGB8_ALPHA8
            case 0x8E8C: return PixelFormat::BC7_UNORM_BLOCK;    // GL_COMPRESSED_RGBA_BPTC_UNORM
            case 0x8E8D: return PixelFormat::BC7_SRGB_BLOCK;     // GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
            case 0x93B0: return PixelFormat::ASTC_4x4_UNORM_BLOCK;
            case 0x93D0: return PixelFormat::ASTC_4x4_SRGB_BLOCK;
            case 0x93B7: return PixelFormat::ASTC_8x8_UNORM_BLOCK;
            case 0x93D7: return PixelFormat::ASTC_8x8_SRGB_BLOCK;
            case 0x93BB: return PixelFormat::ASTC_10x10_UNORM_BLOCK;
            case 0x93DB: return PixelFormat::ASTC_10x10_SRGB_BLOCK;
            case 0x93BD: return PixelFormat::ASTC_12x12_UNORM_BLOCK;
            case 0x93DD: return PixelFormat::ASTC_12x12_SRGB_BLOCK;
            default: break;
            }
            return PixelFormat::UNDEFINED;
        }

        ImageAssetType DeduceType(uint32_t faces, uint32_t arrayElements, uint32_t depth)
        {
            if (faces == 6) {
                return ImageAssetType::TEXTURE_CUBE;
            }
            if (arrayElements > 1) {
                return ImageAssetType::TEXTURE_2D_ARRAY;
            }
            if (depth > 1) {
                return ImageAssetType::TEXTURE_3D;
            }
            return ImageAssetType::TEXTURE_2D;
        }

        ImageObjectPtr MakeUncompressed(uint32_t width, uint32_t height, uint32_t layers, uint32_t mipLevels, PixelFormat fmt, ImageAssetType type, CookImageSource &out)
        {
            auto image        = std::make_shared<ImageObject>();
            image->width      = width;
            image->height     = height;
            image->depth      = layers;
            image->components = GetNumComp(fmt);
            image->format     = fmt;
            image->pixelSize  = GetBytePerComp(fmt) * image->components;
            image->type       = ImageType::IMAGE_2D;
            image->mips.resize(mipLevels);

            out.image      = image;
            out.assetType  = type;
            return image;
        }

        bool ParseKtx1(const std::vector<uint8_t> &bytes, CookImageSource &out)
        {
            const uint32_t glInternalFormat = ReadU32(bytes, 28);
            const uint32_t width            = ReadU32(bytes, 36);
            const uint32_t height           = ReadU32(bytes, 40);
            const uint32_t depth            = ReadU32(bytes, 44);
            const uint32_t arrayElements    = ReadU32(bytes, 48);
            const uint32_t faces            = ReadU32(bytes, 52);
            uint32_t       mipLevels        = ReadU32(bytes, 56);
            const uint32_t kvBytes          = ReadU32(bytes, 60);

            const PixelFormat fmt = MapGlInternalFormat(glInternalFormat);
            if (fmt == PixelFormat::UNDEFINED || width == 0) {
                LOG_E(TAG, "unsupported KTX1 format 0x%x", glInternalFormat);
                return false;
            }
            mipLevels                = std::max(1u, mipLevels);
            const uint32_t layers    = std::max(1u, arrayElements) * std::max(1u, faces);
            const ImageAssetType type = DeduceType(faces, arrayElements, depth);
            const bool compressed    = GetImageFormatInfo(fmt).isCompressed;

            if (compressed) {
                out.precompressed  = true;
                out.asset.format   = fmt;
                out.asset.type     = type;
                out.asset.width    = width;
                out.asset.height   = height;
                out.asset.depth    = std::max(1u, depth);
                out.asset.mipLevels = mipLevels;
                out.asset.arrayLayers = layers;
            } else {
                MakeUncompressed(width, height, layers, mipLevels, fmt, type, out);
            }

            size_t offset = 64 + kvBytes;
            for (uint32_t mip = 0; mip < mipLevels; ++mip) {
                uint32_t imageSize = ReadU32(bytes, offset);
                offset += 4;

                const uint32_t mipW = MipDim(width, mip);
                const uint32_t mipH = MipDim(height, mip);
                const uint32_t mipD = layers > 1 ? layers : MipDim(std::max(1u, depth), mip);

                if (imageSize == 0) {
                    const uint32_t sliceH = mipD > 1 ? MipDim(height, mip) : mipH;
                    imageSize = compressed ? static_cast<uint32_t>(GetImageSlicePitch(fmt, mipW, sliceH))
                                           : static_cast<uint32_t>(GetImageSlicePitch(fmt, mipW, sliceH));
                }

                if (compressed) {
                    for (uint32_t layer = 0; layer < layers; ++layer) {
                        if (offset + imageSize > bytes.size()) {
                            return false;
                        }
                        ImageSliceHeader slice = {};
                        slice.offset   = static_cast<uint32_t>(out.asset.rawData.size());
                        slice.size     = imageSize;
                        slice.mipLevel = mip;
                        slice.layer    = layer;
                        out.asset.slices.push_back(slice);
                        out.asset.rawData.insert(out.asset.rawData.end(), bytes.begin() + offset, bytes.begin() + offset + imageSize);
                        offset = Align4(offset + imageSize);
                    }
                } else {
                    auto &image   = out.image;
                    auto &mipData = image->mips[mip];
                    mipData       = ImageMipData::Create(mipW, mipH, layers, image->pixelSize);
                    const uint32_t layerStride = mipW * mipH * image->pixelSize;
                    for (uint32_t layer = 0; layer < layers; ++layer) {
                        if (offset + imageSize > bytes.size()) {
                            return false;
                        }
                        const uint32_t copySize = std::min(imageSize, layerStride);
                        std::memcpy(mipData.data.get() + layer * layerStride, bytes.data() + offset, copySize);
                        offset = Align4(offset + imageSize);
                    }
                }
            }
            return true;
        }

        bool ParseKtx2(const std::vector<uint8_t> &bytes, CookImageSource &out)
        {
            if (bytes.size() < 80) {
                return false;
            }
            const uint32_t vkFormat      = ReadU32(bytes, 12);
            const uint32_t width         = ReadU32(bytes, 20);
            const uint32_t height        = ReadU32(bytes, 24);
            const uint32_t depth         = ReadU32(bytes, 28);
            const uint32_t layerCount    = ReadU32(bytes, 32);
            const uint32_t faceCount     = ReadU32(bytes, 36);
            uint32_t       levelCount    = ReadU32(bytes, 40);
            const uint32_t superCompression = ReadU32(bytes, 44);

            if (superCompression != 0) {
                LOG_E(TAG, "KTX2 supercompressionScheme %u unsupported", superCompression);
                return false;
            }

            const PixelFormat fmt = MapVkFormat(vkFormat);
            if (fmt == PixelFormat::UNDEFINED || width == 0) {
                LOG_E(TAG, "unsupported KTX2 vkFormat %u", vkFormat);
                return false;
            }
            levelCount             = std::max(1u, levelCount);
            const uint32_t layers  = std::max(1u, layerCount) * std::max(1u, faceCount);
            const ImageAssetType type = DeduceType(faceCount, layerCount, depth);
            const bool compressed  = GetImageFormatInfo(fmt).isCompressed;

            if (compressed) {
                out.precompressed     = true;
                out.asset.format      = fmt;
                out.asset.type        = type;
                out.asset.width       = width;
                out.asset.height      = height;
                out.asset.depth       = std::max(1u, depth);
                out.asset.mipLevels   = levelCount;
                out.asset.arrayLayers = layers;
            } else {
                MakeUncompressed(width, height, layers, levelCount, fmt, type, out);
            }

            const size_t indexBase = 80;
            for (uint32_t mip = 0; mip < levelCount; ++mip) {
                const uint64_t byteOffset = ReadU64(bytes, indexBase + mip * 24);
                const uint64_t byteLength = ReadU64(bytes, indexBase + mip * 24 + 8);

                const uint32_t mipW = MipDim(width, mip);
                const uint32_t mipH = MipDim(height, mip);

                if (compressed) {
                    const uint64_t perLayer = byteLength / layers;
                    for (uint32_t layer = 0; layer < layers; ++layer) {
                        ImageSliceHeader slice = {};
                        slice.offset   = static_cast<uint32_t>(out.asset.rawData.size());
                        slice.size     = static_cast<uint32_t>(perLayer);
                        slice.mipLevel = mip;
                        slice.layer    = layer;
                        out.asset.slices.push_back(slice);

                        const size_t srcOffset = static_cast<size_t>(byteOffset) + layer * perLayer;
                        if (srcOffset + perLayer > bytes.size()) {
                            return false;
                        }
                        out.asset.rawData.insert(out.asset.rawData.end(), bytes.begin() + srcOffset, bytes.begin() + srcOffset + perLayer);
                    }
                } else {
                    auto &image    = out.image;
                    auto &mipData  = image->mips[mip];
                    mipData        = ImageMipData::Create(mipW, mipH, layers, image->pixelSize);
                    const uint32_t layerStride = mipW * mipH * image->pixelSize;
                    for (uint32_t layer = 0; layer < layers; ++layer) {
                        const size_t srcOffset = static_cast<size_t>(byteOffset) + layer * layerStride;
                        if (srcOffset + layerStride > bytes.size()) {
                            return false;
                        }
                        std::memcpy(mipData.data.get() + layer * layerStride, bytes.data() + srcOffset, layerStride);
                    }
                }
            }
            return true;
        }

    } // namespace

    ImageObjectPtr LoadStbImage(const std::vector<uint8_t> &bytes, bool isHdr)
    {
        int x        = 0;
        int y        = 0;
        int channels = 0;

        if (isHdr) {
            float *data = stbi_loadf_from_memory(bytes.data(), static_cast<int>(bytes.size()), &x, &y, &channels, 4);
            if (data == nullptr) {
                LOG_E(TAG, "stb HDR decode failed");
                return {};
            }
            auto image = ImageObject::CreateImage2D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), PixelFormat::RGBA32_SFLOAT);
            image->FillMip0(reinterpret_cast<const uint8_t *>(data), static_cast<uint32_t>(x * y * 4 * sizeof(float)));
            stbi_image_free(data);
            return image;
        }

        stbi_uc *data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &x, &y, &channels, 4);
        if (data == nullptr) {
            LOG_E(TAG, "stb decode failed");
            return {};
        }
        auto image = ImageObject::CreateImage2D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), PixelFormat::RGBA8_UNORM);
        image->FillMip0(data, static_cast<uint32_t>(x * y * 4));
        stbi_image_free(data);
        return image;
    }

    bool LoadKtx(const std::vector<uint8_t> &bytes, CookImageSource &out)
    {
        if (bytes.size() < 12) {
            return false;
        }
        if (std::memcmp(bytes.data(), KTX1_ID, 12) == 0) {
            return ParseKtx1(bytes, out);
        }
        if (std::memcmp(bytes.data(), KTX2_ID, 12) == 0) {
            return ParseKtx2(bytes, out);
        }
        LOG_E(TAG, "not a KTX file");
        return false;
    }

} // namespace sky::aurora::cook
