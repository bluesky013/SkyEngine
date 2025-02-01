//
// Created by blues on 2025/1/29.
//

#pragma once

#include <builder/render/image/ImageProcess.h>

namespace sky::builder {

//    struct BufferImageInfo {
//        uint32_t offset  = 0;
//        uint32_t stride  = 0;
//        uint32_t width   = 0;
//        uint32_t height  = 0;
//        uint32_t layer   = 1;
//    };

    void InitializeCompressor();
//    void CompressImage(uint8_t *ptr, const BufferImageInfo &copy, std::vector<uint8_t> &out, const CompressOption &options);

    class ImageCompressor : public ImageProcess {
    public:
        struct Payload {
            ImageObjectPtr image;
            CompressedImagePtr compressed;
            CompressOption options;
            uint32_t mip = 0;
        };

        explicit ImageCompressor(const Payload &pd) : payload(pd) {}
        virtual ~ImageCompressor() = default;

        void DoWork() override;

    private:
        Payload payload;
    };

} // namespaces sky::builder
