//
// Created by blues on 2025/2/1.
//

#pragma once

#include <builder/render/image/ImageProcess.h>

struct ktxTexture1;
struct ktxTexture2;

namespace sky::builder {

    class KtxImage {
    public:
        explicit KtxImage(const ImageObjectPtr &image);
        ~KtxImage();

        void SaveToFile(const char* path) const;

    private:
        ktxTexture2 *tex = nullptr;
    };

} // namespace sky::builder
