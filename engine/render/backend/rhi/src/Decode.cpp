//
// Created by Zach Lee on 2022/10/23.
//

#include <rhi/ASTC.h>
#include <rhi/DDS.h>
#include <rhi/Decode.h>
#include <rhi/Stream.h>
#include <unordered_map>
#include <cstring>

namespace sky::rhi {

    std::unordered_map<PixelFormat, ImageFormatInfo> FORMAT_INFO =
        {
            {PixelFormat::R8_UNORM,                  {1, 1, 1, 1, false}},
            {PixelFormat::R8_SRGB,                   {1, 1, 1, 1, false}},
            {PixelFormat::RGBA8_UNORM,               {4, 4, 1, 1, false}},
            {PixelFormat::RGBA8_SRGB,                {4, 4, 1, 1, false}},
            {PixelFormat::BGRA8_UNORM,               {4, 4, 1, 1, false}},
            {PixelFormat::BGRA8_SRGB,                {4, 4, 1, 1, false}},
            {PixelFormat::RGBA16_SFLOAT,             {4, 8, 1, 1, false}},
            {PixelFormat::R16_UNORM,                 {1, 2, 1, 1, false}},
            {PixelFormat::R32_SFLOAT,                {1, 4, 1, 1, false}},
            {PixelFormat::RG32_SFLOAT,               {2, 8, 1, 1, false}},
            {PixelFormat::RGB32_SFLOAT,              {3, 12, 1, 1, false}},
            {PixelFormat::RGBA32_SFLOAT,             {4, 16, 1, 1, false}},
            {PixelFormat::D32,                       {1, 4, 1, 1, false, true, false}},
            {PixelFormat::D24_S8,                    {2, 4, 1, 1, false, true, true}},
            {PixelFormat::D32_S8,                    {2, 5, 1, 1, false, true, true}},
            {PixelFormat::BC1_RGB_UNORM_BLOCK,       {1, 8, 4, 4, true}},
            {PixelFormat::BC1_RGB_SRGB_BLOCK,        {1, 8, 4, 4, true}},
            {PixelFormat::BC1_RGBA_UNORM_BLOCK,      {1, 8, 4, 4, true}},
            {PixelFormat::BC1_RGBA_SRGB_BLOCK,       {1, 8, 4, 4, true}},
            {PixelFormat::BC2_UNORM_BLOCK,           {1, 16, 4, 4, true}},
            {PixelFormat::BC2_SRGB_BLOCK,            {1, 16, 4, 4, true}},
            {PixelFormat::BC3_UNORM_BLOCK,           {1, 16, 4, 4, true}},
            {PixelFormat::BC3_SRGB_BLOCK,            {1, 16, 4, 4, true}},
            {PixelFormat::BC4_UNORM_BLOCK,           {1, 8, 4, 4, true}},
            {PixelFormat::BC4_SNORM_BLOCK,           {1, 8, 4, 4, true}},
            {PixelFormat::BC5_UNORM_BLOCK,           {1, 16, 4, 4, true}},
            {PixelFormat::BC5_SNORM_BLOCK,           {1, 16, 4, 4, true}},
            {PixelFormat::BC6H_UFLOAT_BLOCK,         {1, 16, 4, 4, true}},
            {PixelFormat::BC6H_SFLOAT_BLOCK,         {1, 16, 4, 4, true}},
            {PixelFormat::BC7_UNORM_BLOCK,           {1, 16, 4, 4, true}},
            {PixelFormat::BC7_SRGB_BLOCK,            {1, 16, 4, 4, true}},
            {PixelFormat::ETC2_R8G8B8_UNORM_BLOCK,   {1, 8, 4, 4, true}},
            {PixelFormat::ETC2_R8G8B8_SRGB_BLOCK,    {1, 8, 4, 4, true}},
            {PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK, {1, 8, 4, 4, true}},
            {PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK,  {1, 8, 4, 4, true}},
            {PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK, {1, 16, 4, 4, true}},
            {PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK,  {1, 16, 4, 4, true}},
            {PixelFormat::ASTC_4x4_UNORM_BLOCK,      {1, 16, 4, 4, true}},
            {PixelFormat::ASTC_4x4_SRGB_BLOCK,       {1, 16, 4, 4, true}},
            {PixelFormat::ASTC_8x8_UNORM_BLOCK,      {1, 16, 8,  8,  true }},
            {PixelFormat::ASTC_8x8_SRGB_BLOCK,       {1, 16, 8,  8,  true }},
            {PixelFormat::ASTC_10x10_UNORM_BLOCK,    {1, 16, 10, 10, true}},
            {PixelFormat::ASTC_10x10_SRGB_BLOCK,     {1, 16, 10, 10, true}},
            {PixelFormat::ASTC_12x12_UNORM_BLOCK,    {1, 16, 12, 12, true}},
            {PixelFormat::ASTC_12x12_SRGB_BLOCK,     {1, 16, 12, 12, true}},
    };

    std::unordered_map<DXGI_FORMAT, PixelFormat> DXGI_FORMAT_TABLE =
            {
                {DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM, PixelFormat::RGBA8_UNORM},
                {DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, PixelFormat::RGBA8_SRGB},
                {DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM, PixelFormat::BGRA8_UNORM},
                {DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, PixelFormat::BGRA8_SRGB},
                {DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT, PixelFormat::RGBA16_SFLOAT},
                {DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM, PixelFormat::BC1_RGBA_UNORM_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM_SRGB, PixelFormat::BC1_RGBA_SRGB_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM, PixelFormat::BC2_UNORM_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM_SRGB, PixelFormat::BC2_SRGB_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM, PixelFormat::BC3_UNORM_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM_SRGB, PixelFormat::BC3_SRGB_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC4_UNORM, PixelFormat::BC4_UNORM_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC4_SNORM, PixelFormat::BC4_SNORM_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC5_UNORM, PixelFormat::BC5_UNORM_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC5_SNORM, PixelFormat::BC4_SNORM_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM, PixelFormat::BC7_UNORM_BLOCK},
                {DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM_SRGB, PixelFormat::BC7_SRGB_BLOCK},
    };

    ImageFormatInfo *GetImageInfoByFormat(PixelFormat format)
    {
        auto iter = FORMAT_INFO.find(format);
        return iter == FORMAT_INFO.end() ? nullptr : &iter->second;
    }

    AspectFlags GetAspectFlagsByFormat(PixelFormat format, bool useDepth, bool useStencil)
    {
        auto *formatInfo = GetImageInfoByFormat(format);
        if (!formatInfo->hasDepth && !formatInfo->hasStencil) {
            return rhi::AspectFlagBit::COLOR_BIT;
        }
        AspectFlags aspectMask;
        if (formatInfo->hasDepth && useDepth) {
            aspectMask |= rhi::AspectFlagBit::DEPTH_BIT;
        }
        if (formatInfo->hasStencil && useStencil) {
            aspectMask |= rhi::AspectFlagBit::STENCIL_BIT;
        }
        return aspectMask;
    }

    void ProcessASTC(uint8_t *input, uint64_t size, Image::Descriptor &imageDesc, std::vector<ImageUploadRequest> &requests)
    {
        ASTCHeader* header = reinterpret_cast<ASTCHeader *>(input);
        if (header->magic[0] != 0x13 || header->magic[1] != 0xAB || header->magic[2] != 0xA1 || header->magic[3] == 0x5C) {
            return;
        }
    }

    uint32_t ProcessDDSHeader(uint8_t *input, uint64_t size, Image::Descriptor &imageDesc)
    {
        auto *content = reinterpret_cast<DDSContent *>(input);
        if (content == nullptr || content->magic != 0x20534444) {
            return 0;
        }

        uint32_t offset = sizeof(DWORD) + content->header.dwSize;
        const auto *pf = &content->header.ddspf;
        auto dwFlags = pf->dwFlags;

        DXGI_FORMAT format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
        if ((dwFlags & DDPF_FOURCC) != 0u) {
            if (content->header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', '1', '0')) {
                auto *contentExt = reinterpret_cast<DDSContentExt *>(input);
                offset += sizeof(DDS_HEADER_DXT10);
                format = contentExt->header10.dxgiFormat;
            }
        } else {

            if ((dwFlags & DDPF_RGB) != 0u) {
                switch (pf->dwRGBBitCount) {
                case 32:
                {
                    if (pf->dwRBitMask == 0x000000ff &&
                        pf->dwGBitMask == 0x0000ff00 &&
                        pf->dwBBitMask == 0x00ff0000 &&
                        pf->dwABitMask == 0xff000000) {
                        format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
                    }
                    if (pf->dwRBitMask == 0x00ff0000 &&
                        pf->dwGBitMask == 0x0000ff00 &&
                        pf->dwBBitMask == 0x000000ff &&
                        pf->dwABitMask == 0xff000000) {
                        format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
                    }
                }
                default:
                    break;
                };
            }
        }
        {
            auto iter = DXGI_FORMAT_TABLE.find(format);
            if (iter == DXGI_FORMAT_TABLE.end()) {
                return 0;
            }
            imageDesc.format = iter->second;
        }


        imageDesc.extent.width  = content->header.dwWidth;
        imageDesc.extent.height = content->header.dwHeight;
        imageDesc.mipLevels     = content->header.dwMipMapCount;

        if ((content->header.dwCaps2 & DDSCAPS2_CUBEMAP) != 0u) {
            imageDesc.arrayLayers = 6;
        }

        return offset;
    }

    void ProcessDDS(uint8_t *input, uint64_t size, Image::Descriptor &imageDesc, std::vector<ImageUploadRequest> &requests)
    {
        uint32_t offset = ProcessDDSHeader(input, size, imageDesc);

        auto iter = FORMAT_INFO.find(imageDesc.format);
        if (iter == FORMAT_INFO.end()) {
            return;
        }

        uint32_t blockWidth = iter->second.blockWidth;
        uint32_t blockHeight = iter->second.blockHeight;
        uint32_t blockSize = iter->second.blockSize;

        ImageUploadRequest request = {};
        request.source = new RawPtrStream(input);
        request.imageExtent.width  = imageDesc.extent.width;
        request.imageExtent.height = imageDesc.extent.height;

        for (uint32_t j = 0; j < imageDesc.arrayLayers; ++j) {
            for (uint32_t i = 0; i < imageDesc.mipLevels; ++i) {
                auto width  = std::max(imageDesc.extent.width >> i, 1U);
                auto height = std::max(imageDesc.extent.height >> i, 1U);

                uint32_t rowLength   = (width + blockWidth - 1) / blockWidth;
                uint32_t imageHeight = (height + blockHeight - 1) / blockHeight;
                uint32_t currentSize = rowLength * imageHeight * blockSize;
                request.imageExtent.depth  = 1;
                request.source             = new RawPtrStream(input);
                request.offset             = offset;
                request.size               = currentSize;
                request.mipLevel           = i;
                request.layer              = j;
                offset += currentSize;
                requests.emplace_back(request);
            }
        }
    }
}
