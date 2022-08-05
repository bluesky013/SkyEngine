//
// Created by Zach Lee on 2022/5/7.
//


#include <render/resources/Texture.h>
#include <render/Render.h>
#include <render/DriverManager.h>

namespace sky {

    void Texture::SetSampler(drv::SamplerPtr samp)
    {
        sampler = samp;
    }

    void Texture::SetImageView(drv::ImageViewPtr view)
    {
        imageView = view;
    }

    bool Texture::IsValid() const
    {
        return !!imageView && !!sampler;
    }

    RDTexturePtr Texture::CreateFromImage(RDImagePtr image, const Texture::Descriptor& desc)
    {
        auto texture = std::make_shared<Texture>(desc);
        drv::ImageView::Descriptor viewDesc = {};
        viewDesc.format = image->GetFormat();
        viewDesc.subResourceRange.baseMipLevel = desc.baseMipLevel;
        viewDesc.subResourceRange.levelCount = desc.levelCount;
        viewDesc.subResourceRange.baseArrayLayer = desc.baseArrayLayer;
        viewDesc.subResourceRange.layerCount = desc.layerCount;
        auto rhiImage = image->GetRHIImage();
        auto imageView = drv::ImageView::CreateImageView(rhiImage, viewDesc);
        auto sampler= Render::Get()->GetDefaultSampler();

        texture->SetImageView(imageView);
        texture->SetSampler(sampler);
        texture->sourceImage = image;
        return texture;
    }
}