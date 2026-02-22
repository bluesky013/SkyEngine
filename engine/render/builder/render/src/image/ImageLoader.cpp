//
// Created by blues on 2025/10/3.
//

#include <builder/render/image/ImageLoader.h>

namespace sky::builder {

    void ImageLoaderManager::Register(ImageLoader* loader)
    {
        loaders.emplace_back(loader);
    }

    ImageObjectPtr ImageLoaderManager::LoadImage(const FilePtr& file, const std::string& ext)
    {
        ImageObjectPtr res;
        for (auto &loader : loaders) {

            if (loader->SupportFile(ext)) {
                res = loader->LoadImage(file);
                if (res) {
                    break;
                }
            }
        }

        return res;
    }

    ImageObjectPtr ImageLoaderManager::LoadImage(const BinaryDataPtr& bin, const std::string& ext)
    {
        ImageObjectPtr res;
        for (auto &loader : loaders) {

            if (loader->SupportFile(ext)) {
                res = loader->LoadImage(bin);
                if (res) {
                    break;
                }
            }
        }

        return res;
    }

} // namespace sky::builder