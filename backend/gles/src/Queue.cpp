//
// Created by Zach on 2023/1/31.
//

#include <gles/Queue.h>
#include <gles/CommandBuffer.h>
#include <gles/Core.h>

namespace sky::gles {

    bool Queue::Init(const Context::Descriptor &cfg, rhi::QueueType t)
    {
        type = t;
        context = std::make_unique<Context>();
        CreateTask([this, cfg]() {
            context->Init(cfg);
        });
        return true;
    }

    rhi::TransferTaskHandle Queue::UploadImage(const rhi::ImagePtr &image, const rhi::ImageUploadRequest &request)
    {
        auto glesImage = std::static_pointer_cast<Image>(image);
        auto handle = CreateTask([glesImage, request]() {
            auto &desc = glesImage->GetDescriptor();
            auto &fmt = GetInternalFormat(desc.format);
            glBindTexture(GL_TEXTURE_2D, glesImage->GetNativeHandle());
            CHECK(glTexSubImage2D(GL_TEXTURE_2D,
                request.mipLevel,
                request.imageOffset.x, request.imageOffset.y,
                request.imageExtent.width, request.imageExtent.height,
                fmt.format,
                fmt.type,
                reinterpret_cast<const GLvoid *>(request.data + request.offset)));
        });
        return handle;
    };
}

