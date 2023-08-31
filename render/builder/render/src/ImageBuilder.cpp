//
// Created by Zach Lee on 2023/2/26.
//

#include <builder/render/ImageBuilder.h>
#include <render/adaptor/assets/ImageAsset.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <filesystem>

namespace sky::builder {

    void ImageBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        if (!request.buildKey.empty() && request.buildKey != std::string(KEY)) {
            return;
        }
        int x,y,n;
        unsigned char *data = stbi_load(request.fullPath.c_str(), &x, &y, &n, 0);
        if (data == nullptr) {
            return;
        }
        AssetManager *am = AssetManager::Get();
        std::filesystem::path outPath(request.outDir);
        outPath.append("images");
        std::filesystem::create_directories(outPath);
        outPath.append(request.name);
        outPath.replace_extension(".image");

        auto asset = am->CreateAsset<Texture>(outPath.make_preferred().string());
        auto &imageData = asset->Data();
        imageData.width = static_cast<uint32_t>(x);
        imageData.height = static_cast<uint32_t>(y);
        imageData.format = n == 4 ? rhi::PixelFormat::RGBA8_UNORM : rhi::PixelFormat::RGB8_UNORM;
        imageData.rawData.resize(1);

        size_t size = x * y * n;
        imageData.rawData[0].resize(size);
        memcpy(imageData.rawData[0].data(), data, size);

        result.products.emplace_back(BuildProduct{KEY, asset->GetUuid()});
        am->SaveAsset(asset);

        stbi_image_free(data);
    }
}