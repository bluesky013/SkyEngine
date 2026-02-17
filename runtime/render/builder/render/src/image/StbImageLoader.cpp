//
// Created by blues on 2025/10/3.
//

#include <builder/render/image/StbImageLoader.h>
#include <stb_image.h>

namespace sky::builder {

    ImageObjectPtr StbImageLoader::LoadImage(const FilePtr& file)
    {
        std::vector<uint8_t> rawData;
        file->ReadBin(rawData);

        int x;
        int y;
        int channel;
        unsigned char *data = nullptr;
        data = stbi_load_from_memory(static_cast<const stbi_uc*>(rawData.data()), static_cast<int>(rawData.size()), &x, &y, &channel, 4);

        ImageObjectPtr image = ImageObject::CreateImage2D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), rhi::PixelFormat::RGBA8_UNORM);
        image->FillMip0(data, static_cast<uint32_t>(x * y * 4));
        stbi_image_free(data);

        return image;
    }

    ImageObjectPtr StbImageLoader::LoadImage(const BinaryDataPtr& bin)
    {
        int x;
        int y;
        int channel;
        unsigned char *data = nullptr;
        data = stbi_load_from_memory(static_cast<const stbi_uc*>(bin->Data()), static_cast<int>(bin->Size()), &x, &y, &channel, 4);

        ImageObjectPtr image = ImageObject::CreateImage2D(static_cast<uint32_t>(x), static_cast<uint32_t>(y), rhi::PixelFormat::RGBA8_UNORM);
        image->FillMip0(data, static_cast<uint32_t>(x * y * 4));
        stbi_image_free(data);

        return image;
    }

    bool StbImageLoader::SupportFile(const std::string& ext) const
    {
        return ext == ".jpg" || ext == ".jpeg";
    }

} // sky::builder