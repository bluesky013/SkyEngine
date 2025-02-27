//
// Created by blues on 2025/1/30.
//

#include <builder/render/image/ImageCompressor.h>
#include <builder/render/image/ImageProcess.h>

#include <core/util/DynamicModule.h>
#include <core/math/MathUtil.h>
#include <rhi/Decode.h>

#include <ispc_texcomp/ispc_texcomp.h>

#include <memory>

using PFN_GetProfile_ultrafast       = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_veryfast        = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_fast            = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_basic           = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_slow            = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_alpha_ultrafast = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_alpha_veryfast  = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_alpha_fast      = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_alpha_basic     = void(*)(bc7_enc_settings* settings);
using PFN_GetProfile_alpha_slow      = void(*)(bc7_enc_settings* settings);

using PFN_GetProfile_astc_fast       = void(*)(astc_enc_settings* settings, int block_width, int block_height);
using PFN_GetProfile_astc_alpha_fast = void(*)(astc_enc_settings* settings, int block_width, int block_height);
using PFN_GetProfile_astc_alpha_slow = void(*)(astc_enc_settings* settings, int block_width, int block_height);

using PFN_CompressBlocksBC1  = void(*)(const rgba_surface* src, uint8_t* dst);
using PFN_CompressBlocksBC3  = void(*)(const rgba_surface* src, uint8_t* dst);
using PFN_CompressBlocksBC4  = void(*)(const rgba_surface* src, uint8_t* dst);
using PFN_CompressBlocksBC5  = void(*)(const rgba_surface* src, uint8_t* dst);
using PFN_CompressBlocksBC6H = void(*)(const rgba_surface* src, uint8_t* dst, bc6h_enc_settings* settings);
using PFN_CompressBlocksBC7  = void(*)(const rgba_surface* src, uint8_t* dst, bc7_enc_settings* settings);
using PFN_CompressBlocksETC1 = void(*)(const rgba_surface* src, uint8_t* dst, etc_enc_settings* settings);
using PFN_CompressBlocksASTC = void(*)(const rgba_surface* src, uint8_t* dst, astc_enc_settings* settings);

namespace sky::builder {

    PFN_GetProfile_ultrafast       S_GetProfile_BC7_ultrafast       = nullptr;
    PFN_GetProfile_veryfast        S_GetProfile_BC7_veryfast        = nullptr;
    PFN_GetProfile_fast            S_GetProfile_BC7_fast            = nullptr;
    PFN_GetProfile_basic           S_GetProfile_BC7_basic           = nullptr;
    PFN_GetProfile_slow            S_GetProfile_BC7_slow            = nullptr;
    PFN_GetProfile_alpha_ultrafast S_GetProfile_BC7_alpha_ultrafast = nullptr;
    PFN_GetProfile_alpha_veryfast  S_GetProfile_BC7_alpha_veryfast  = nullptr;
    PFN_GetProfile_alpha_fast      S_GetProfile_BC7_alpha_fast      = nullptr;
    PFN_GetProfile_alpha_basic     S_GetProfile_BC7_alpha_basic     = nullptr;
    PFN_GetProfile_alpha_slow      S_GetProfile_BC7_alpha_slow      = nullptr;

    PFN_GetProfile_astc_fast        S_GetProfile_astc_fast           = nullptr;
    PFN_GetProfile_astc_alpha_fast  S_GetProfile_astc_alpha_fast     = nullptr;
    PFN_GetProfile_astc_alpha_slow  S_GetProfile_astc_alpha_slow = nullptr;

    PFN_CompressBlocksBC1          S_CompressBlocksBC1  = nullptr;
    PFN_CompressBlocksBC3          S_CompressBlocksBC3  = nullptr;
    PFN_CompressBlocksBC4          S_CompressBlocksBC4  = nullptr;
    PFN_CompressBlocksBC5          S_CompressBlocksBC5  = nullptr;
    PFN_CompressBlocksBC6H         S_CompressBlocksBC6H = nullptr;
    PFN_CompressBlocksBC7          S_CompressBlocksBC7  = nullptr;
    PFN_CompressBlocksETC1         S_CompressBlocksETC1 = nullptr;
    PFN_CompressBlocksASTC         S_CompressBlocksASTC = nullptr;

    std::unique_ptr<DynamicModule> ISPC_COMP_MODULE;

    void InitializeCompressor()
    {
        if (!ISPC_COMP_MODULE) {
            ISPC_COMP_MODULE = std::make_unique<DynamicModule>("ispc_texcomp");
            ISPC_COMP_MODULE->Load();
            S_GetProfile_BC7_ultrafast       = reinterpret_cast<PFN_GetProfile_ultrafast      >(ISPC_COMP_MODULE->GetAddress("GetProfile_ultrafast"));
            S_GetProfile_BC7_veryfast        = reinterpret_cast<PFN_GetProfile_veryfast       >(ISPC_COMP_MODULE->GetAddress("GetProfile_veryfast"));
            S_GetProfile_BC7_fast            = reinterpret_cast<PFN_GetProfile_fast           >(ISPC_COMP_MODULE->GetAddress("GetProfile_fast"));
            S_GetProfile_BC7_basic           = reinterpret_cast<PFN_GetProfile_basic          >(ISPC_COMP_MODULE->GetAddress("GetProfile_basic"));
            S_GetProfile_BC7_slow            = reinterpret_cast<PFN_GetProfile_slow           >(ISPC_COMP_MODULE->GetAddress("GetProfile_slow"));
            S_GetProfile_BC7_alpha_ultrafast = reinterpret_cast<PFN_GetProfile_alpha_ultrafast>(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_ultrafast"));
            S_GetProfile_BC7_alpha_veryfast  = reinterpret_cast<PFN_GetProfile_alpha_veryfast >(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_veryfast"));
            S_GetProfile_BC7_alpha_fast      = reinterpret_cast<PFN_GetProfile_alpha_fast     >(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_fast"));
            S_GetProfile_BC7_alpha_basic     = reinterpret_cast<PFN_GetProfile_alpha_basic    >(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_basic"));
            S_GetProfile_BC7_alpha_slow      = reinterpret_cast<PFN_GetProfile_alpha_slow     >(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_slow"));

            S_GetProfile_astc_fast           = reinterpret_cast<PFN_GetProfile_astc_fast      >(ISPC_COMP_MODULE->GetAddress("GetProfile_astc_fast"));
            S_GetProfile_astc_alpha_fast     = reinterpret_cast<PFN_GetProfile_astc_alpha_fast>(ISPC_COMP_MODULE->GetAddress("GetProfile_astc_alpha_fast"));
            S_GetProfile_astc_alpha_slow     = reinterpret_cast<PFN_GetProfile_astc_alpha_slow>(ISPC_COMP_MODULE->GetAddress("GetProfile_astc_alpha_slow"));

            S_CompressBlocksBC1       = reinterpret_cast<PFN_CompressBlocksBC1 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC1"));
            S_CompressBlocksBC3       = reinterpret_cast<PFN_CompressBlocksBC3 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC3"));
            S_CompressBlocksBC4       = reinterpret_cast<PFN_CompressBlocksBC4 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC4"));
            S_CompressBlocksBC5       = reinterpret_cast<PFN_CompressBlocksBC5 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC5"));
            S_CompressBlocksBC6H      = reinterpret_cast<PFN_CompressBlocksBC6H>(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC6H"));
            S_CompressBlocksBC7       = reinterpret_cast<PFN_CompressBlocksBC7 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC7"));
            S_CompressBlocksETC1      = reinterpret_cast<PFN_CompressBlocksETC1>(ISPC_COMP_MODULE->GetAddress("CompressBlocksETC1"));
            S_CompressBlocksASTC      = reinterpret_cast<PFN_CompressBlocksASTC>(ISPC_COMP_MODULE->GetAddress("CompressBlocksASTC"));
        }
    }

    void ComPressASTC(const rgba_surface &input, uint8_t *out, const CompressOption &options)
    {
        int blockWidth = 1;
        int blockHeight = 1;
        switch (options.targetFormat) {
        case rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK:;
        case rhi::PixelFormat::ASTC_4x4_SRGB_BLOCK:
            blockWidth = 4;
            blockHeight = 4;
            break;
        case rhi::PixelFormat::ASTC_8x8_UNORM_BLOCK:
        case rhi::PixelFormat::ASTC_8x8_SRGB_BLOCK:
            blockWidth = 8;
            blockHeight = 8;
            break;
        case rhi::PixelFormat::ASTC_10x10_UNORM_BLOCK:
        case rhi::PixelFormat::ASTC_10x10_SRGB_BLOCK:
            blockWidth = 10;
            blockHeight = 10;
            break;
        case rhi::PixelFormat::ASTC_12x12_UNORM_BLOCK:
        case rhi::PixelFormat::ASTC_12x12_SRGB_BLOCK:
            blockWidth = 12;
            blockHeight = 12;
            break;
        default:
            break;
        }

        astc_enc_settings settings = {};
        switch (options.quality) {
        case Quality::ULTRA_FAST:
        case Quality::VERY_FAST:
        case Quality::FAST:
            if (options.hasAlpha) {
                S_GetProfile_astc_alpha_fast(&settings, blockWidth, blockHeight);
            } else {
                S_GetProfile_astc_fast(&settings, blockWidth, blockHeight);
            }
            break;
        case Quality::BASIC:
        case Quality::SLOW:
            if (options.hasAlpha) {
                S_GetProfile_astc_alpha_slow(&settings, blockWidth, blockHeight);
            } else {
                S_GetProfile_astc_fast(&settings, blockWidth, blockHeight);
            }
            break;
        }

        S_CompressBlocksASTC(&input, out, &settings);
    }

    void CompressBC7(const rgba_surface &input, uint8_t *out, const CompressOption &options)
    {
        bc7_enc_settings settings = {};
        switch (options.quality) {
        case Quality::ULTRA_FAST:
            if (options.hasAlpha) {
                S_GetProfile_BC7_alpha_ultrafast(&settings);
            } else {
                S_GetProfile_BC7_ultrafast(&settings);
            }
            break;
        case Quality::VERY_FAST:
            if (options.hasAlpha) {
                S_GetProfile_BC7_alpha_veryfast(&settings);
            } else {
                S_GetProfile_BC7_veryfast(&settings);
            }
            break;
        case Quality::FAST:
            if (options.hasAlpha) {
                S_GetProfile_BC7_alpha_fast(&settings);
            } else {
                S_GetProfile_BC7_fast(&settings);
            }
            break;
        case Quality::BASIC:
            if (options.hasAlpha) {
                S_GetProfile_BC7_alpha_basic(&settings);
            } else {
                S_GetProfile_BC7_basic(&settings);
            }
            break;
        case Quality::SLOW:
            if (options.hasAlpha) {
                S_GetProfile_BC7_alpha_slow(&settings);
            } else {
                S_GetProfile_BC7_slow(&settings);
            }
            break;
        }

        S_CompressBlocksBC7(&input, out, &settings);
    }

    void CompressImage(const rgba_surface &input,  uint8_t *out, const CompressOption &options)
    {
        if (options.targetFormat == rhi::PixelFormat::BC7_UNORM_BLOCK){
            CompressBC7(input, out, options);
        } else if (options.targetFormat == rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK)
        {
            ComPressASTC(input, out, options);
        }
    }

//    void CompressImage(uint8_t *ptr, const BufferImageInfo &copy, std::vector<uint8_t> &out, const CompressOption &options)
//    {
//        rgba_surface input = {};
//        input.ptr = ptr + copy.offset;
//        input.width  = static_cast<int32_t>(copy.width);
//        input.height = static_cast<int32_t>(copy.height);
//        input.stride = static_cast<int32_t>(copy.stride);
//
//        uint32_t width = copy.width;
//        uint32_t height = copy.height;
//
//        const auto *formatInfo = rhi::GetImageInfoByFormat(options.targetFormat);
//        uint32_t rowLength   = Ceil(width, formatInfo->blockWidth);
//        uint32_t imageHeight = Ceil(height, formatInfo->blockHeight);
//        uint32_t blockNum = rowLength * imageHeight;
//        uint32_t dataSize = blockNum * formatInfo->blockSize;
//
//        out.resize(dataSize);
//        CompressImage(input, out.data(), options);
//    }

    void ImageCompressor::DoWork()
    {
        auto *dstPf = rhi::GetImageInfoByFormat(payload.options.targetFormat);
        if (dstPf == nullptr) {
            return;
        }
        SKY_ASSERT(payload.mip < payload.image->mips.size());
        SKY_ASSERT(payload.mip < payload.compressed->mips.size());

        const auto &srcMipData = payload.image->mips[payload.mip];

        rgba_surface input = {};
        input.ptr = srcMipData.data.get();
        input.width  = static_cast<int32_t>(srcMipData.width);
        input.height = static_cast<int32_t>(srcMipData.height);
        input.stride = srcMipData.rowPitch;

        uint32_t rowLength   = Ceil(srcMipData.width, dstPf->blockWidth);
        uint32_t imageHeight = Ceil(srcMipData.height, dstPf->blockHeight);
        uint32_t blockNum = rowLength * imageHeight;
        uint32_t dataSize = blockNum * dstPf->blockSize;

        auto &dstMipData = payload.compressed->mips[payload.mip];
        dstMipData.rowPitch = rowLength * dstPf->blockSize;;
        dstMipData.dataLength = dataSize;
        dstMipData.data = std::make_unique<uint8_t []>(dataSize);
        CompressImage(input, dstMipData.data.get(), payload.options);
    }

} // namespace sky::builder