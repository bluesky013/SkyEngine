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
        rhi::TransferTaskHandle Upload(const uint8_t *ptr, uint64_t size, rhi::Queue &queue);

        bool CheckExtent(uint32_t width, uint32_t height, uint32_t depth = 1) const;

        const rhi::ImageViewPtr &GetImageView() const { return imageView; }
        const rhi::ImagePtr GetImage() const { return image; }
    protected:
        rhi::Device *device = nullptr;
        rhi::Image::Descriptor imageDesc = {};

        rhi::ImagePtr image;
        rhi::SamplerPtr sampler;
        rhi::ImageViewPtr imageView;
    };
    using RDTexturePtr = std::shared_ptr<Texture>;

    class TextureCube : public Texture {
    public:
        TextureCube() = default;
        ~TextureCube() override = default;

        bool Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t mipLevel);
    };
    using RDTextureCubePtr = std::shared_ptr<TextureCube>;

    class Texture2D : public Texture {
    public:
        Texture2D() = default;
        ~Texture2D() override = default;

        bool Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t mipLevel);
    };
    using RDTexture2DPtr = std::shared_ptr<Texture2D>;

    class Texture3D : public Texture {
    public:
        Texture3D() = default;
        ~Texture3D() override = default;

        bool Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t depth);
    };
    using RDTexture3DPtr = std::shared_ptr<Texture3D>;
} // namespace sky
