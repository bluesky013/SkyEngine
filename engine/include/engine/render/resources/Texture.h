//
// Created by Zach Lee on 2022/5/7.
//


#pragma once

#include <engine/render/resources/RenderResource.h>
#include <vulkan/Sampler.h>
#include <vulkan/ImageView.h>

namespace sky {

    class Texture : public RenderResource {
    public:
        struct Descriptor {
            uint32_t baseMipLevel   = 0;
            uint32_t levelCount     = 1;
            uint32_t baseArrayLayer = 0;
            uint32_t layerCount     = 1;
        };

        Texture(const Descriptor& desc) : descriptor(desc)
        {
        }

        ~Texture() = default;

        void SetSampler(drv::SamplerPtr sampler);

        void SetImageView(drv::ImageViewPtr imageView);

        drv::SamplerPtr GetSampler() const { return sampler; }

        drv::ImageViewPtr GetImageView() const { return imageView; }

    private:
        Descriptor descriptor;
        drv::SamplerPtr sampler;
        drv::ImageViewPtr imageView;
    };
    using RDTexturePtr = std::shared_ptr<Texture>;

}