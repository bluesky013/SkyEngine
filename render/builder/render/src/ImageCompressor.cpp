//
// Created by blues on 2023/9/11.
//

#include <builder/render/ImageCompressor.h>

#include <memory>

#include <core/util/DynamicModule.h>
#include <core/math/MathUtil.h>
#include <rhi/Decode.h>
#include <ispc_texcomp/ispc_texcomp.h>

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

using PFN_CompressBlocksBC1  = void(*)(const rgba_surface* src, uint8_t* dst);
using PFN_CompressBlocksBC3  = void(*)(const rgba_surface* src, uint8_t* dst);
using PFN_CompressBlocksBC4  = void(*)(const rgba_surface* src, uint8_t* dst);
using PFN_CompressBlocksBC5  = void(*)(const rgba_surface* src, uint8_t* dst);
using PFN_CompressBlocksBC6H = void(*)(const rgba_surface* src, uint8_t* dst, bc6h_enc_settings* settings);
using PFN_CompressBlocksBC7  = void(*)(const rgba_surface* src, uint8_t* dst, bc7_enc_settings* settings);
using PFN_CompressBlocksETC1 = void(*)(const rgba_surface* src, uint8_t* dst, etc_enc_settings* settings);
using PFN_CompressBlocksASTC = void(*)(const rgba_surface* src, uint8_t* dst, astc_enc_settings* settings);

namespace sky::builder {

    PFN_GetProfile_ultrafast       GetProfile_BC7_ultrafast       = nullptr;
    PFN_GetProfile_veryfast        GetProfile_BC7_veryfast        = nullptr;
    PFN_GetProfile_fast            GetProfile_BC7_fast            = nullptr;
    PFN_GetProfile_basic           GetProfile_BC7_basic           = nullptr;
    PFN_GetProfile_slow            GetProfile_BC7_slow            = nullptr;
    PFN_GetProfile_alpha_ultrafast GetProfile_BC7_alpha_ultrafast = nullptr;
    PFN_GetProfile_alpha_veryfast  GetProfile_BC7_alpha_veryfast  = nullptr;
    PFN_GetProfile_alpha_fast      GetProfile_BC7_alpha_fast      = nullptr;
    PFN_GetProfile_alpha_basic     GetProfile_BC7_alpha_basic     = nullptr;
    PFN_GetProfile_alpha_slow      GetProfile_BC7_alpha_slow      = nullptr;

    PFN_CompressBlocksBC1          CompressBlocksBC1  = nullptr;
    PFN_CompressBlocksBC3          CompressBlocksBC3  = nullptr;
    PFN_CompressBlocksBC4          CompressBlocksBC4  = nullptr;
    PFN_CompressBlocksBC5          CompressBlocksBC5  = nullptr;
    PFN_CompressBlocksBC6H         CompressBlocksBC6H = nullptr;
    PFN_CompressBlocksBC7          CompressBlocksBC7  = nullptr;
    PFN_CompressBlocksETC1         CompressBlocksETC1 = nullptr;
    PFN_CompressBlocksASTC         CompressBlocksASTC = nullptr;

    std::unique_ptr<DynamicModule> ISPC_COMP_MODULE;

    void InitializeCompressor()
    {
        if (!ISPC_COMP_MODULE) {
            ISPC_COMP_MODULE = std::make_unique<DynamicModule>("ispc_texcomp");
            ISPC_COMP_MODULE->Load();
            GetProfile_BC7_ultrafast       = reinterpret_cast<PFN_GetProfile_ultrafast      >(ISPC_COMP_MODULE->GetAddress("GetProfile_ultrafast"));
            GetProfile_BC7_veryfast        = reinterpret_cast<PFN_GetProfile_veryfast       >(ISPC_COMP_MODULE->GetAddress("GetProfile_veryfast"));
            GetProfile_BC7_fast            = reinterpret_cast<PFN_GetProfile_fast           >(ISPC_COMP_MODULE->GetAddress("GetProfile_fast"));
            GetProfile_BC7_basic           = reinterpret_cast<PFN_GetProfile_basic          >(ISPC_COMP_MODULE->GetAddress("GetProfile_basic"));
            GetProfile_BC7_slow            = reinterpret_cast<PFN_GetProfile_slow           >(ISPC_COMP_MODULE->GetAddress("GetProfile_slow"));
            GetProfile_BC7_alpha_ultrafast = reinterpret_cast<PFN_GetProfile_alpha_ultrafast>(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_ultrafast"));
            GetProfile_BC7_alpha_veryfast  = reinterpret_cast<PFN_GetProfile_alpha_veryfast >(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_veryfast"));
            GetProfile_BC7_alpha_fast      = reinterpret_cast<PFN_GetProfile_alpha_fast     >(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_fast"));
            GetProfile_BC7_alpha_basic     = reinterpret_cast<PFN_GetProfile_alpha_basic    >(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_basic"));
            GetProfile_BC7_alpha_slow      = reinterpret_cast<PFN_GetProfile_alpha_slow     >(ISPC_COMP_MODULE->GetAddress("GetProfile_alpha_slow"));

            CompressBlocksBC1       = reinterpret_cast<PFN_CompressBlocksBC1 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC1"));
            CompressBlocksBC3       = reinterpret_cast<PFN_CompressBlocksBC3 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC3"));
            CompressBlocksBC4       = reinterpret_cast<PFN_CompressBlocksBC4 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC4"));
            CompressBlocksBC5       = reinterpret_cast<PFN_CompressBlocksBC5 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC5"));
            CompressBlocksBC6H      = reinterpret_cast<PFN_CompressBlocksBC6H>(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC6H"));
            CompressBlocksBC7       = reinterpret_cast<PFN_CompressBlocksBC7 >(ISPC_COMP_MODULE->GetAddress("CompressBlocksBC7"));
            CompressBlocksETC1      = reinterpret_cast<PFN_CompressBlocksETC1>(ISPC_COMP_MODULE->GetAddress("CompressBlocksETC1"));
            CompressBlocksASTC      = reinterpret_cast<PFN_CompressBlocksASTC>(ISPC_COMP_MODULE->GetAddress("CompressBlocksASTC"));
        }
    }

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

    void CompressBC7(const rgba_surface &input, uint8_t *out, const CompressOption &options)
    {
        bc7_enc_settings settings = {};
        switch (options.quality) {
        case Quality::ULTRA_FAST:
            GetProfile_BC7_ultrafast(&settings);
            break;
        case Quality::VERY_FAST:
            GetProfile_BC7_veryfast(&settings);
            break;
        case Quality::FAST:
            GetProfile_BC7_fast(&settings);
            break;
        case Quality::BASIC:
            GetProfile_BC7_basic(&settings);
            break;
        case Quality::SLOW:
            GetProfile_BC7_slow(&settings);
            break;
        }

        CompressBlocksBC7(&input, out, &settings);
    }

    void CompressImage(const rgba_surface &input,  uint8_t *out, const CompressOption &options)
    {
        if (options.targetFormat == rhi::PixelFormat::BC7_UNORM_BLOCK) {
            CompressBC7(input, out, options);
        }
    }

    void CompressImage(uint8_t *ptr, const BufferImageInfo &copy, uint8_t *&out, const CompressOption &options)
    {
        rgba_surface input = {};
        input.ptr = ptr + copy.offset;
        input.width  = static_cast<int32_t>(copy.width);
        input.height = static_cast<int32_t>(copy.height);
        input.stride = static_cast<int32_t>(copy.stride);

        uint32_t level = GetMipLevel(copy.width, copy.height);
        uint32_t width = copy.width;
        uint32_t height = copy.height;

        const auto *formatInfo = rhi::GetImageInfoByFormat(options.targetFormat);
        uint32_t rowLength   = Ceil(width, formatInfo->blockWidth);
        uint32_t imageHeight = Ceil(height, formatInfo->blockHeight);
        uint32_t blockNum = rowLength * imageHeight;
        uint32_t srcSize  = blockNum * formatInfo->blockSize;
    }

} // namespace sky::builder