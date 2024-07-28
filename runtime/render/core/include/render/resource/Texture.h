//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <rhi/Device.h>
#include <rhi/Queue.h>
#include <render/RenderResource.h>
#include <core/file/FileSystem.h>
#include <fstream>

namespace sky {

    enum class TextureType : uint32_t {
        TEXTURE_2D,
        TEXTURE_3D,
        TEXTURE_CUBE
    };

    struct ImageData {
        std::vector<rhi::ImageUploadRequest> slices;
    };

    class Texture : public RenderResource {
    public:
        Texture();
        ~Texture() override;

        // upload
        void SetUploadStream(ImageData&& stream);
        void UploadMeshData();


        rhi::TransferTaskHandle Upload(const FilePtr &archive, rhi::Queue &queue, uint32_t offset);
        rhi::TransferTaskHandle Upload(const std::string &path, rhi::Queue &queue, uint32_t offset);
        rhi::TransferTaskHandle Upload(const uint8_t *ptr, uint64_t size, rhi::Queue &queue);

        bool CheckExtent(uint32_t width, uint32_t height, uint32_t depth = 1) const;

        const rhi::ImageViewPtr &GetImageView() const { return imageView; }
        const rhi::ImagePtr &GetImage() const { return image; }
    protected:
        rhi::Device *device = nullptr;
        rhi::Image::Descriptor imageDesc = {};

        rhi::ImagePtr image;
        rhi::SamplerPtr sampler;
        rhi::ImageViewPtr imageView;

        ImageData data;
        rhi::TransferTaskHandle uploadHandle {};
    };
    using RDTexturePtr = CounterPtr<Texture>;

    class TextureCube : public Texture {
    public:
        TextureCube() = default;
        ~TextureCube() override = default;

        bool Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t mipLevel);
    };
    using RDTextureCubePtr = CounterPtr<TextureCube>;

    class Texture2D : public Texture {
    public:
        Texture2D() = default;
        ~Texture2D() override = default;

        bool Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t mipLevel);
    };
    using RDTexture2DPtr = CounterPtr<Texture2D>;

    class Texture3D : public Texture {
    public:
        Texture3D() = default;
        ~Texture3D() override = default;

        bool Init(rhi::PixelFormat format, uint32_t width, uint32_t height, uint32_t depth);
    };
    using RDTexture3DPtr = CounterPtr<Texture3D>;
} // namespace sky
