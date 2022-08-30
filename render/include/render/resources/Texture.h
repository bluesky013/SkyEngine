//
// Created by Zach Lee on 2022/5/7.
//

#pragma once

#include <render/resources/Image.h>
#include <render/resources/RenderResource.h>
#include <vulkan/ImageView.h>
#include <vulkan/Sampler.h>

namespace sky {

    class Texture : public RenderResource {
    public:
        struct Descriptor {
            uint32_t baseMipLevel   = 0;
            uint32_t levelCount     = 1;
            uint32_t baseArrayLayer = 0;
            uint32_t layerCount     = 1;
        };

        Texture(const Descriptor &desc) : descriptor(desc)
        {
        }

        ~Texture() = default;

        void SetSampler(drv::SamplerPtr sampler);

        void SetImageView(drv::ImageViewPtr imageView);

        drv::SamplerPtr GetSampler() const
        {
            return sampler;
        }

        drv::ImageViewPtr GetImageView() const
        {
            return imageView;
        }

        bool IsValid() const override;

        static std::shared_ptr<Texture> CreateFromImage(RDImagePtr image, const Texture::Descriptor &desc);

    private:
        Descriptor        descriptor;
        RDImagePtr        sourceImage;
        drv::SamplerPtr   sampler;
        drv::ImageViewPtr imageView;
    };
    using RDTexturePtr = std::shared_ptr<Texture>;

} // namespace sky