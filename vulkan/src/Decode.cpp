//
// Created by Zach Lee on 2022/10/23.
//

#include <vulkan/Decode.h>
#include <vulkan/ASTC.h>
#include <vulkan/DDS.h>

namespace sky::vk {

    std::unordered_map<VkFormat, ImageFormatInfo> FORMAT_INFO =
        {
            {VK_FORMAT_R8G8B8A8_UNORM,            {4,  1,  1,  false}},
            {VK_FORMAT_R8G8B8A8_SRGB,             {4,  1,  1,  false}},
            {VK_FORMAT_BC1_RGB_UNORM_BLOCK,       {8,  4,  4,  true }},
            {VK_FORMAT_BC1_RGB_SRGB_BLOCK,        {8,  4,  4,  true }},
            {VK_FORMAT_BC1_RGBA_UNORM_BLOCK,      {8,  4,  4,  true }},
            {VK_FORMAT_BC1_RGBA_SRGB_BLOCK,       {8,  4,  4,  true }},
            {VK_FORMAT_BC2_UNORM_BLOCK,           {16, 4,  4,  true }},
            {VK_FORMAT_BC2_SRGB_BLOCK,            {16, 4,  4,  true }},
            {VK_FORMAT_BC3_UNORM_BLOCK,           {16, 4,  4,  true }},
            {VK_FORMAT_BC3_SRGB_BLOCK,            {16, 4,  4,  true }},
            {VK_FORMAT_BC4_UNORM_BLOCK,           {8,  4,  4,  true }},
            {VK_FORMAT_BC4_SNORM_BLOCK,           {8,  4,  4,  true }},
            {VK_FORMAT_BC5_UNORM_BLOCK,           {16, 4,  4,  true }},
            {VK_FORMAT_BC5_SNORM_BLOCK,           {16, 4,  4,  true }},
            {VK_FORMAT_BC6H_UFLOAT_BLOCK,         {16, 4,  4,  true }},
            {VK_FORMAT_BC6H_SFLOAT_BLOCK,         {16, 4,  4,  true }},
            {VK_FORMAT_BC7_UNORM_BLOCK,           {16, 4,  4,  true }},
            {VK_FORMAT_BC7_SRGB_BLOCK,            {16, 4,  4,  true }},
            {VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,   {8,  4,  4,  true }},
            {VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,    {8,  4,  4,  true }},
            {VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, {8,  4,  4,  true }},
            {VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,  {8,  4,  4,  true }},
            {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, {16, 4,  4,  true }},
            {VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,  {16, 4,  4,  true }},
            {VK_FORMAT_ASTC_4x4_UNORM_BLOCK,      {16, 4,  4,  true }},
            {VK_FORMAT_ASTC_4x4_SRGB_BLOCK,       {16, 4,  4,  true }},
            {VK_FORMAT_ASTC_5x4_UNORM_BLOCK,      {16, 5,  4,  true }},
            {VK_FORMAT_ASTC_5x4_SRGB_BLOCK,       {16, 5,  4,  true }},
            {VK_FORMAT_ASTC_5x5_UNORM_BLOCK,      {16, 5,  5,  true }},
            {VK_FORMAT_ASTC_5x5_SRGB_BLOCK,       {16, 5,  5,  true }},
            {VK_FORMAT_ASTC_6x5_UNORM_BLOCK,      {16, 6,  5,  true }},
            {VK_FORMAT_ASTC_6x5_SRGB_BLOCK,       {16, 6,  5,  true }},
            {VK_FORMAT_ASTC_6x6_UNORM_BLOCK,      {16, 6,  6,  true }},
            {VK_FORMAT_ASTC_6x6_SRGB_BLOCK,       {16, 6,  6,  true }},
            {VK_FORMAT_ASTC_8x5_UNORM_BLOCK,      {16, 8,  5,  true }},
            {VK_FORMAT_ASTC_8x5_SRGB_BLOCK,       {16, 8,  5,  true }},
            {VK_FORMAT_ASTC_8x6_UNORM_BLOCK,      {16, 8,  6,  true }},
            {VK_FORMAT_ASTC_8x6_SRGB_BLOCK,       {16, 8,  6,  true }},
            {VK_FORMAT_ASTC_8x8_UNORM_BLOCK,      {16, 8,  8,  true }},
            {VK_FORMAT_ASTC_8x8_SRGB_BLOCK,       {16, 8,  8,  true }},
            {VK_FORMAT_ASTC_10x5_UNORM_BLOCK,     {16, 10, 5,  true }},
            {VK_FORMAT_ASTC_10x5_SRGB_BLOCK,      {16, 10, 5,  true }},
            {VK_FORMAT_ASTC_10x6_UNORM_BLOCK,     {16, 10, 6,  true }},
            {VK_FORMAT_ASTC_10x6_SRGB_BLOCK,      {16, 10, 6,  true }},
            {VK_FORMAT_ASTC_10x8_UNORM_BLOCK,     {16, 10, 8,  true }},
            {VK_FORMAT_ASTC_10x8_SRGB_BLOCK,      {16, 10, 8,  true }},
            {VK_FORMAT_ASTC_10x10_UNORM_BLOCK,    {16, 10, 10, true }},
            {VK_FORMAT_ASTC_10x10_SRGB_BLOCK,     {16, 10, 10, true }},
            {VK_FORMAT_ASTC_12x10_UNORM_BLOCK,    {16, 12, 10, true }},
            {VK_FORMAT_ASTC_12x10_SRGB_BLOCK,     {16, 12, 10, true }},
            {VK_FORMAT_ASTC_12x12_UNORM_BLOCK,    {16, 12, 12, true }},
            {VK_FORMAT_ASTC_12x12_SRGB_BLOCK,     {16, 12, 12, true }},
    };

    std::unordered_map<DXGI_FORMAT, VkFormat> DXGI_FORMAT_TABLE =
        {
            {DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,      VK_FORMAT_R8G8B8A8_UNORM      },
            {DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, VK_FORMAT_R8G8B8A8_SRGB       },
            {DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM,           VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
            {DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM_SRGB,      VK_FORMAT_BC1_RGBA_SRGB_BLOCK },
            {DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM,           VK_FORMAT_BC2_UNORM_BLOCK     },
            {DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM_SRGB,      VK_FORMAT_BC2_SRGB_BLOCK      },
            {DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM,           VK_FORMAT_BC3_UNORM_BLOCK     },
            {DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM_SRGB,      VK_FORMAT_BC3_SRGB_BLOCK      },
            {DXGI_FORMAT::DXGI_FORMAT_BC4_UNORM,           VK_FORMAT_BC4_UNORM_BLOCK     },
            {DXGI_FORMAT::DXGI_FORMAT_BC4_SNORM,           VK_FORMAT_BC4_SNORM_BLOCK     },
            {DXGI_FORMAT::DXGI_FORMAT_BC5_UNORM,           VK_FORMAT_BC5_UNORM_BLOCK     },
            {DXGI_FORMAT::DXGI_FORMAT_BC5_SNORM,           VK_FORMAT_BC4_SNORM_BLOCK     },
            {DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM,           VK_FORMAT_BC7_UNORM_BLOCK     },
            {DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM_SRGB,      VK_FORMAT_BC7_SRGB_BLOCK      },
    };

    ImageFormatInfo *GetImageInfoByFormat(VkFormat format)
    {
        auto iter = FORMAT_INFO.find(format);
        return iter == FORMAT_INFO.end() ? nullptr : &iter->second;
    }

    void ProcessASTC(uint8_t *input, uint64_t size, Image::Descriptor &imageDesc, std::vector<ImageUploadRequest> &requests)
    {
        ASTCHeader* header = reinterpret_cast<ASTCHeader *>(input);
        if (header->magic[0] != 0x13 || header->magic[1] != 0xAB || header->magic[2] != 0xA1 || header->magic[3] == 0x5C) {
            return;
        }
    }

    void ProcessDDS(uint8_t *input, uint64_t size, Image::Descriptor &imageDesc, std::vector<ImageUploadRequest> &requests)
    {
        DDSContent *content = reinterpret_cast<DDSContent *>(input);
        if (content->magic != 0x20534444) {
            return;
        }

        uint32_t offset = sizeof(DWORD) + content->header.dwSize;
        if (content->header.ddspf.dwFlags & DDPF_FOURCC) {
            if (content->header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', '1', '0')) {
                DDSContentExt *contentExt = reinterpret_cast<DDSContentExt *>(input);
                offset += sizeof(DDS_HEADER_DXT10);

                auto iter = DXGI_FORMAT_TABLE.find(contentExt->header10.dxgiFormat);
                if (iter == DXGI_FORMAT_TABLE.end()) {
                    return;
                }
                imageDesc.format = iter->second;
            }
        }

        imageDesc.extent.width  = content->header.dwWidth;
        imageDesc.extent.height = content->header.dwHeight;
        imageDesc.mipLevels     = content->header.dwMipMapCount;

        auto iter = FORMAT_INFO.find(imageDesc.format);
        if (iter == FORMAT_INFO.end()) {
            return;
        }
        uint32_t width  = imageDesc.extent.width;
        uint32_t height = imageDesc.extent.height;

        uint32_t blockWidth = iter->second.blockWidth;
        uint32_t blockHeight = iter->second.blockHeight;
        uint32_t blockSize = iter->second.blockSize;

        ImageUploadRequest request = {};
        request.data = input;
        request.layer = 0;

        for (uint32_t i = 0; i < imageDesc.mipLevels; ++i) {
            request.offset = offset;
            request.mipLevel = i;

            uint32_t rowLength   = (width + blockWidth - 1) / blockWidth;
            uint32_t imageHeight = (height + blockHeight - 1) / blockHeight;
            uint32_t currentSize = rowLength * imageHeight * blockSize;

            offset += currentSize;
            width = std::max(width >> 1, 1U);
            height = std::max(height >> 1, 1U);

            requests.emplace_back(request);
        }
    }
}
