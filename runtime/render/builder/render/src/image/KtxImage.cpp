//
// Created by blues on 2025/2/1.
//

#include <builder/render/image/KtxImage.h>

#include <ktx.h>

namespace sky::builder {

    static constexpr ktx_uint32_t VK_FORMAT_UNDEFINED = 0;
    static constexpr ktx_uint32_t VK_FORMAT_R8G8B8A8_UNORM = 37;
    static constexpr ktx_uint32_t VK_FORMAT_R32_SFLOAT = 100;
    static constexpr ktx_uint32_t VK_FORMAT_R32G32_SFLOAT = 103;
    static constexpr ktx_uint32_t VK_FORMAT_R32G32B32_SFLOAT = 106;
    static constexpr ktx_uint32_t VK_FORMAT_R32G32B32A32_SFLOAT = 109;

    static constexpr ktx_uint32_t GL_RGBA8 = 0x8058;

    static const ktx_uint32_t VK_FORMAT_F32Table[] = {
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT
    };

    static ktx_uint32_t GetPixelFormatKtx(PixelType type, uint32_t components)
    {
        if (type == PixelType::U8) {
            SKY_ASSERT(components == 4);
            return GL_RGBA8;
        }
        return 0;
    }

    static ktx_uint32_t GetPixelFormatKtx2(PixelType type, uint32_t components)
    {
        if (type == PixelType::U8) {
            SKY_ASSERT(components == 4);
            return VK_FORMAT_R8G8B8A8_UNORM;
        } else if (type == PixelType::Float) {
            return VK_FORMAT_F32Table[components - 1];
        }
        return VK_FORMAT_UNDEFINED;
    }

    KtxImage::KtxImage(const ImageObjectPtr &image)
    {
        ktxTextureCreateInfo createInfo = {};
        createInfo.baseWidth = image->width;
        createInfo.baseHeight = image->height;
        createInfo.baseDepth = image->depth;
        createInfo.numDimensions = static_cast<uint32_t>(image->type) + 1;
        createInfo.numLayers = image->type == rhi::ImageType::IMAGE_2D ? image->depth : 1;
        createInfo.numLevels = static_cast<uint32_t>(image->mips.size());
        createInfo.numFaces = 1;
        createInfo.isArray = false;
        createInfo.vkFormat = GetPixelFormatKtx2(image->pixelType, image->components);

        ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &tex);

        for (uint32_t i = 0; i < image->mips.size(); ++i) {
            const auto &mip = image->mips[i];
            KTX_error_code result = ktxTexture_SetImageFromMemory(ktxTexture(tex), i, 0u, 0, mip.data.get(), mip.dataLength);
            SKY_ASSERT(result == KTX_SUCCESS)
        }
    }

    KtxImage::~KtxImage()
    {
        ktxTexture_Destroy(ktxTexture(tex));
    }

    void KtxImage::SaveToFile(const char* path) const
    {
        KTX_error_code result = ktxTexture_WriteToNamedFile(ktxTexture(tex), path);
        SKY_ASSERT(result == KTX_SUCCESS)
    }

} // namespace sky::builder