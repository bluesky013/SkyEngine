//
// Created by Zach on 2023/1/31.
//

#include <gles/Image.h>
#include <gles/Device.h>
#include <gles/Core.h>

namespace sky::gles {
    static const rhi::ImageUsageFlags USAGE_COLOR_OR_DS = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::DEPTH_STENCIL;

    static bool UsageSampled(rhi::ImageUsageFlags usage) { return (usage & rhi::ImageUsageFlagBit::SAMPLED) == rhi::ImageUsageFlagBit::SAMPLED; }
    static bool UsageStorage(rhi::ImageUsageFlags usage) { return (usage & rhi::ImageUsageFlagBit::STORAGE) == rhi::ImageUsageFlagBit::STORAGE; }
    static bool UsageAttachment(rhi::ImageUsageFlags usage) { return (usage & USAGE_COLOR_OR_DS) == USAGE_COLOR_OR_DS; }

    Image::~Image()
    {
        if (texId != 0) {
            if (renderBuffer) {
                glDeleteRenderbuffers(1, &texId);
            } else {
                glDeleteTextures(1, &texId);
            }
        }
    }

    bool Image::Init(const Descriptor &desc)
    {
        imageDesc = desc;
        auto &fmt = GetInternalFormat(desc.format);
        auto &feature = GetFormatFeature(desc.format);

        bool usageSample = UsageSampled(desc.usage);
        bool usageAttachment = UsageAttachment(desc.usage);

        renderBuffer = feature.renderBuffer && usageAttachment && !usageSample;
        if (renderBuffer) {
            glGenRenderbuffers(1, &texId);
            glBindRenderbuffer(GL_RENDERBUFFER, texId);
            if (imageDesc.samples > 1) {
                CHECK(glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, imageDesc.samples, fmt.internal, imageDesc.extent.width, imageDesc.extent.height));
            } else {
                CHECK(glRenderbufferStorage(GL_RENDERBUFFER, fmt.internal, imageDesc.extent.width, imageDesc.extent.height));
            }
        } else {
            CHECK(glGenTextures(1, &texId));
            CHECK(glBindTexture(GL_TEXTURE_2D_ARRAY, texId));
//            CHECK(glTexStorage3D(GL_TEXTURE_2D_ARRAY, imageDesc.mipLevels, fmt.internal, imageDesc.extent.width, imageDesc.extent.height, imageDesc.arrayLayers));

            for (uint32_t i = 0; i < imageDesc.mipLevels; ++i) {
                glTexImage3D(GL_TEXTURE_2D_ARRAY, i, fmt.internal, imageDesc.extent.width, imageDesc.extent.height, imageDesc.arrayLayers, 0, fmt.format, fmt.type, nullptr);
            }

//            CHECK(glBindTexture(GL_TEXTURE_CUBE_MAP, texId));

        }
        return true;
    }

}
