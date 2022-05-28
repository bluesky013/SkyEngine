//
// Created by Zach Lee on 2022/5/7.
//


#include <engine/render/resources/Texture.h>

namespace sky {

    void Texture::SetSampler(drv::SamplerPtr samp)
    {
        sampler = samp;
    }

    void Texture::SetImageView(drv::ImageViewPtr view)
    {
        imageView = view;
    }
}