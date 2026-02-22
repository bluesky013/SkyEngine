//
// Created by blues on 2025/10/3.
//

#pragma once

#include <builder/render/image/ImageLoader.h>

namespace sky::builder {

    class StbImageLoader : public ImageLoader {
    public:
        StbImageLoader() = default;
        ~StbImageLoader() override = default;

        ImageObjectPtr LoadImage(const FilePtr& file) override;
        ImageObjectPtr LoadImage(const BinaryDataPtr& bin) override;
        bool SupportFile(const std::string& ext) const override;
    };

} // sky::builder
