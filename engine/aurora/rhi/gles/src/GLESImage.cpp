//
// Created on 2026/04/01.
//

#include <GLESImage.h>
#include <GLESLoader.h>
#include <GLESDevice.h>
#include <GLESConversion.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraGL";

namespace sky::aurora {

    static const ImageUsageFlags USAGE_ATTACHMENT = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::DEPTH_STENCIL;

    GLESImage::GLESImage(GLESDevice &dev)
        : device(dev)
    {
    }

    GLESImage::~GLESImage()
    {
        if (texId != 0) {
            if (renderBuffer) {
                glDeleteRenderbuffers(1, &texId);
            } else {
                glDeleteTextures(1, &texId);
            }
        }
    }

    bool GLESImage::Init(const Descriptor &desc)
    {
        const auto &fmt = FromPixelFormat(desc.format);
        if (fmt.internalFormat == 0) {
            LOG_E(TAG, "unsupported pixel format for GLES image");
            return false;
        }

        const bool sampled    = (desc.usage & ImageUsageFlagBit::SAMPLED) != ImageUsageFlagBit::NONE;
        const bool attachment = (desc.usage & USAGE_ATTACHMENT) != ImageUsageFlagBit::NONE;
        const bool multiSample = desc.samples > SampleCount::X1;
        const GLsizei sampleCount = static_cast<GLsizei>(desc.samples);

        // Use renderbuffer when only attachment (not sampled)
        renderBuffer = attachment && !sampled;

        if (renderBuffer) {
            glGenRenderbuffers(1, &texId);
            if (texId == 0) {
                LOG_E(TAG, "glGenRenderbuffers failed");
                return false;
            }
            glBindRenderbuffer(GL_RENDERBUFFER, texId);
            if (multiSample) {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount,
                    fmt.internalFormat, desc.extent.width, desc.extent.height);
            } else {
                glRenderbufferStorage(GL_RENDERBUFFER, fmt.internalFormat,
                    desc.extent.width, desc.extent.height);
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            target = GL_RENDERBUFFER;
            return true;
        }

        // Texture path
        glGenTextures(1, &texId);
        if (texId == 0) {
            LOG_E(TAG, "glGenTextures failed");
            return false;
        }

        const bool is3D    = desc.imageType == ImageType::IMAGE_3D;
        const bool isArray = desc.arrayLayers > 1;

        if (is3D) {
            target = GL_TEXTURE_3D;
            glBindTexture(target, texId);
            glTexStorage3D(target, desc.mipLevels, fmt.internalFormat,
                desc.extent.width, desc.extent.height, desc.extent.depth);
        } else if (isArray) {
            if (multiSample) {
                target = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
                glBindTexture(target, texId);
                glTexStorage3DMultisample(target, sampleCount, fmt.internalFormat,
                    desc.extent.width, desc.extent.height, desc.arrayLayers, GL_FALSE);
            } else {
                target = GL_TEXTURE_2D_ARRAY;
                glBindTexture(target, texId);
                glTexStorage3D(target, desc.mipLevels, fmt.internalFormat,
                    desc.extent.width, desc.extent.height, desc.arrayLayers);
            }
        } else {
            if (multiSample) {
                target = GL_TEXTURE_2D_MULTISAMPLE;
                glBindTexture(target, texId);
                glTexStorage2DMultisample(target, sampleCount, fmt.internalFormat,
                    desc.extent.width, desc.extent.height, GL_FALSE);
            } else {
                target = GL_TEXTURE_2D;
                glBindTexture(target, texId);
                glTexStorage2D(target, desc.mipLevels, fmt.internalFormat,
                    desc.extent.width, desc.extent.height);
            }
        }

        glBindTexture(target, 0);
        return true;
    }

} // namespace sky::aurora
