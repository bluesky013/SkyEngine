//
// Created by blues on 2025/10/3.
//

#pragma once

#include <builder/render/image/ImageProcess.h>
#include <core/environment/Singleton.h>
#include <core/archive/BinaryData.h>

namespace sky::builder {

    class ImageLoader {
    public:
        ImageLoader() = default;
        virtual ~ImageLoader() = default;

        virtual ImageObjectPtr LoadImage(const FilePtr& file) = 0;
        virtual ImageObjectPtr LoadImage(const BinaryDataPtr& bin) = 0;
        virtual bool SupportFile(const std::string& ext) const = 0;
    };

    class ImageLoaderManager : public Singleton<ImageLoaderManager> {
    public:
        ImageLoaderManager() = default;
        ~ImageLoaderManager() override = default;

        void Register(ImageLoader* loader);
        ImageObjectPtr LoadImage(const FilePtr& file, const std::string& ext);
        ImageObjectPtr LoadImage(const BinaryDataPtr& bin, const std::string& ext);

    private:
        std::vector<std::unique_ptr<ImageLoader>> loaders;
    };

} // sky::builder