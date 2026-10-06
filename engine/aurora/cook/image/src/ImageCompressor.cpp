//
// Block compression (see ImageCompressor.h).
//

#include <aurora/cook/image/ImageCompressor.h>

#include <core/logger/Logger.h>
#include <core/platform/Platform.h>
#include <core/util/DynamicModule.h>

#include <astcenc.h>
#include <ispc_texcomp/ispc_texcomp.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

static const char *TAG = "AuroraImageCompressor";

namespace sky::aurora::cook {

    namespace {

        // --- ispc_texcomp runtime module (BC7) ---

        using PFN_GetProfile = void (*)(bc7_enc_settings *settings);
        using PFN_CompressBlocksBC7 = void (*)(const rgba_surface *src, uint8_t *dst, bc7_enc_settings *settings);

        PFN_GetProfile        S_BC7_ultrafast       = nullptr;
        PFN_GetProfile        S_BC7_veryfast        = nullptr;
        PFN_GetProfile        S_BC7_fast            = nullptr;
        PFN_GetProfile        S_BC7_basic           = nullptr;
        PFN_GetProfile        S_BC7_slow            = nullptr;
        PFN_GetProfile        S_BC7_alpha_ultrafast = nullptr;
        PFN_GetProfile        S_BC7_alpha_veryfast  = nullptr;
        PFN_GetProfile        S_BC7_alpha_fast      = nullptr;
        PFN_GetProfile        S_BC7_alpha_basic     = nullptr;
        PFN_GetProfile        S_BC7_alpha_slow      = nullptr;
        PFN_CompressBlocksBC7 S_CompressBlocksBC7   = nullptr;

        std::unique_ptr<DynamicModule> ISPC_MODULE;

        void LoadIspcModule()
        {
            if (ISPC_MODULE) {
                return;
            }
            ISPC_MODULE = std::make_unique<DynamicModule>("ispc_texcomp");
            ISPC_MODULE->Load();

            S_BC7_ultrafast       = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_ultrafast"));
            S_BC7_veryfast        = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_veryfast"));
            S_BC7_fast            = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_fast"));
            S_BC7_basic           = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_basic"));
            S_BC7_slow            = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_slow"));
            S_BC7_alpha_ultrafast = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_alpha_ultrafast"));
            S_BC7_alpha_veryfast  = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_alpha_veryfast"));
            S_BC7_alpha_fast      = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_alpha_fast"));
            S_BC7_alpha_basic     = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_alpha_basic"));
            S_BC7_alpha_slow      = reinterpret_cast<PFN_GetProfile>(ISPC_MODULE->GetAddress("GetProfile_alpha_slow"));
            S_CompressBlocksBC7   = reinterpret_cast<PFN_CompressBlocksBC7>(ISPC_MODULE->GetAddress("CompressBlocksBC7"));
        }

        void CompressBC7(const uint8_t *ptr, uint32_t stride, uint32_t width, uint32_t height, uint8_t *out, Quality quality, bool hasAlpha)
        {
            LoadIspcModule();
            if (S_CompressBlocksBC7 == nullptr) {
                LOG_E(TAG, "ispc_texcomp module unavailable; BC7 compression skipped");
                return;
            }

            bc7_enc_settings settings = {};
            PFN_GetProfile   profile  = nullptr;
            switch (quality) {
            case Quality::ULTRA_FAST: profile = hasAlpha ? S_BC7_alpha_ultrafast : S_BC7_ultrafast; break;
            case Quality::VERY_FAST:  profile = hasAlpha ? S_BC7_alpha_veryfast : S_BC7_veryfast; break;
            case Quality::FAST:       profile = hasAlpha ? S_BC7_alpha_fast : S_BC7_fast; break;
            case Quality::BASIC:      profile = hasAlpha ? S_BC7_alpha_basic : S_BC7_basic; break;
            case Quality::SLOW:       profile = hasAlpha ? S_BC7_alpha_slow : S_BC7_slow; break;
            }
            if (profile == nullptr) {
                return;
            }
            profile(&settings);

            rgba_surface surface = {};
            surface.ptr          = const_cast<uint8_t *>(ptr);
            surface.width        = static_cast<int32_t>(width);
            surface.height       = static_cast<int32_t>(height);
            surface.stride       = static_cast<int32_t>(stride);

            S_CompressBlocksBC7(&surface, out, &settings);
        }

        float QualityToAstc(Quality quality)
        {
            switch (quality) {
            case Quality::ULTRA_FAST:
            case Quality::VERY_FAST:
                return ASTCENC_PRE_FAST;
            case Quality::FAST:
                return ASTCENC_PRE_MEDIUM;
            case Quality::BASIC:
                return ASTCENC_PRE_THOROUGH;
            case Quality::SLOW:
                return ASTCENC_PRE_EXHAUSTIVE;
            }
            return ASTCENC_PRE_MEDIUM;
        }

        void CompressASTC(const uint8_t *ptr, uint32_t width, uint32_t height, uint32_t blockSize, bool srgb, uint8_t *out, uint32_t outSize, Quality quality)
        {
            astcenc_config config = {};
            const astcenc_profile profile = srgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR;
            const unsigned int    flags   = ASTCENC_FLG_USE_PERCEPTUAL | ASTCENC_FLG_USE_ALPHA_WEIGHT;

            astcenc_error err = astcenc_config_init(profile, blockSize, blockSize, 1, QualityToAstc(quality), flags, &config);
            if (err != ASTCENC_SUCCESS) {
                LOG_E(TAG, "astcenc_config_init failed: %s", astcenc_get_error_string(err));
                return;
            }

            astcenc_context *context = nullptr;
            err = astcenc_context_alloc(&config, 1, &context);
            if (err != ASTCENC_SUCCESS) {
                LOG_E(TAG, "astcenc_context_alloc failed: %s", astcenc_get_error_string(err));
                return;
            }

            astcenc_image image = {};
            image.dim_x        = width;
            image.dim_y        = height;
            image.dim_z        = 1;
            image.data_type    = ASTCENC_TYPE_U8;
            void *slice        = const_cast<uint8_t *>(ptr);
            image.data         = &slice;

            const astcenc_swizzle swizzle = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
            err = astcenc_compress_image(context, &image, &swizzle, out, outSize, 0);
            if (err != ASTCENC_SUCCESS) {
                LOG_E(TAG, "astcenc_compress_image failed: %s", astcenc_get_error_string(err));
            }
            astcenc_context_free(context);
        }

    } // namespace

    void ImageCompressor::DoWork()
    {
        const auto &targetFormat = payload.config.ResolveFormat();
        const auto &info         = GetImageFormatInfo(targetFormat);
        if (!info.isCompressed) {
            return;
        }

        SKY_ASSERT(payload.mip < payload.image->mips.size());
        SKY_ASSERT(payload.mip < payload.compressed->mips.size());

        const ImageMipData &src = payload.image->mips[payload.mip];
        SKY_ASSERT(src.depth == 1);

        const uint32_t blockW = info.blockWidth;
        const uint32_t blockH = info.blockHeight;

        // Block aligned, edge replicated surface; astcenc requires tightly
        // packed rows and ispc needs block aligned input.
        const uint32_t extW = ((src.width + blockW - 1) / blockW) * blockW;
        const uint32_t extH = ((src.height + blockH - 1) / blockH) * blockH;

        const uint8_t *srcPtr    = src.data.get();
        uint32_t       srcStride = src.rowPitch;
        std::vector<uint8_t> padded;

        if (extW != src.width || extH != src.height) {
            padded.resize(static_cast<size_t>(extW) * extH * 4);
            for (uint32_t y = 0; y < extH; ++y) {
                const uint8_t *row = srcPtr + std::min(y, src.height - 1) * srcStride;
                uint8_t       *dst = padded.data() + static_cast<size_t>(y) * extW * 4;
                for (uint32_t x = 0; x < extW; ++x) {
                    std::memcpy(dst + x * 4, row + std::min(x, src.width - 1) * 4, 4);
                }
            }
            srcPtr    = padded.data();
            srcStride = extW * 4;
        }

        const uint32_t blocksX  = (src.width + blockW - 1) / blockW;
        const uint32_t blocksY  = (src.height + blockH - 1) / blockH;
        const uint32_t dataSize = blocksX * blocksY * info.blockSize;

        auto &dst        = payload.compressed->mips[payload.mip];
        dst.rowPitch     = blocksX * info.blockSize;
        dst.dataLength   = dataSize;
        dst.data         = std::make_unique<uint8_t[]>(dataSize);

        if (payload.config.encode == ImageEncode::BC7) {
            CompressBC7(srcPtr, srcStride, extW, extH, dst.data.get(), payload.config.quality, payload.hasAlpha);
        } else {
            CompressASTC(srcPtr, extW, extH, payload.config.astcBlock, payload.config.srgb, dst.data.get(), dataSize, payload.config.quality);
        }
    }

} // namespace sky::aurora::cook
