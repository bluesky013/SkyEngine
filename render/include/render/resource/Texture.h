//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <rhi/Device.h>
#include <rhi/Queue.h>
#include <fstream>

namespace sky {

    class Texture {
    public:
        Texture();
        virtual ~Texture();

        rhi::TransferTaskHandle Upload(const std::string &path, rhi::Queue &queue, uint32_t offset);
        rhi::TransferTaskHandle Upload(const uint8_t *ptr, uint32_t size, rhi::Queue &queue);

        const rhi::ImageViewPtr &GetImageView() const { return imageView; }
    protected:
        rhi::Device *device = nullptr;
        rhi::Image::Descriptor imageDesc = {};

        rhi::ImagePtr image;
        rhi::SamplerPtr sampler;
        rhi::ImageViewPtr imageView;
    };
    using RDTexturePtr = std::shared_ptr<Texture>;

    class Texture2D : public Texture {
    public:
        Texture2D() = default;
        ~Texture2D() override = default;

        bool Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t mipLevel);
    };
    using RDTexture2DPtr = std::shared_ptr<Texture2D>;

} // namespace sky
