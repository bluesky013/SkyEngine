//
// Created by Zach on 2023/1/31.
//

#include <gles/Image.h>
#include <gles/Device.h>
#include <gles/Core.h>
#include <gles/ImageView.h>
#include <gles/Ext.h>

namespace sky::gles {
    static const rhi::ImageUsageFlags USAGE_COLOR_OR_DS = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::DEPTH_STENCIL;

    static bool UsageSampled(rhi::ImageUsageFlags usage) { return (usage & rhi::ImageUsageFlagBit::SAMPLED) == rhi::ImageUsageFlagBit::SAMPLED; }
    static bool UsageStorage(rhi::ImageUsageFlags usage) { return (usage & rhi::ImageUsageFlagBit::STORAGE) == rhi::ImageUsageFlagBit::STORAGE; }
    static bool UsageAttachment(rhi::ImageUsageFlags usage) { return (usage & USAGE_COLOR_OR_DS) != rhi::ImageUsageFlagBit::NONE; }

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
            CHECK(glGenRenderbuffers(1, &texId));
            CHECK(glBindRenderbuffer(GL_RENDERBUFFER, texId));
            if (imageDesc.samples > rhi::SampleCount::X1) {
                CHECK(RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, static_cast<GLsizei>(imageDesc.samples), fmt.internal, imageDesc.extent.width, imageDesc.extent.height));
            } else {
                CHECK(glRenderbufferStorage(GL_RENDERBUFFER, fmt.internal, imageDesc.extent.width, imageDesc.extent.height));
            }
        } else {
            CHECK(glGenTextures(1, &texId));

            if (imageDesc.arrayLayers == 1) {
                CHECK(glBindTexture(GL_TEXTURE_2D, texId));
                if (imageDesc.samples > rhi::SampleCount::X1) {
                    CHECK(glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, static_cast<GLsizei>(imageDesc.samples), fmt.internal, imageDesc.extent.width, imageDesc.extent.height, GL_FALSE));
                } else {
                    CHECK(glTexStorage2D(GL_TEXTURE_2D, imageDesc.mipLevels, fmt.internal, imageDesc.extent.width, imageDesc.extent.height));
                }
            } else {
                bool is3D = imageDesc.extent.depth != 1;
                auto target = is3D ? GL_TEXTURE_3D : GL_TEXTURE_2D_ARRAY;
                bool depth = is3D ? imageDesc.extent.depth : imageDesc.arrayLayers;

                CHECK(glBindTexture(target, texId));

                if (imageDesc.samples > rhi::SampleCount::X1) {
                    CHECK(glTexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, static_cast<GLsizei>(imageDesc.samples), fmt.internal, imageDesc.extent.width, imageDesc.extent.height, depth, GL_FALSE));
                } else {
                    CHECK(glTexStorage3D(target, imageDesc.mipLevels, fmt.internal, imageDesc.extent.width, imageDesc.extent.height, depth));
                }
            }
        }
        return true;
    }

    rhi::ImageViewPtr Image::CreateView(const rhi::ImageViewDesc &desc)
    {
        ImageViewPtr ret = std::make_shared<ImageView>(device);
        ret->source      = shared_from_this();
        if (!ret->Init(desc)) {
            ret = nullptr;
        }
        return std::static_pointer_cast<rhi::ImageView>(ret);
    }

}
