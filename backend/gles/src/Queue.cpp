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
        state = std::make_unique<PipelineCacheState>();
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
            CHECK(glBindTexture(GL_TEXTURE_2D, glesImage->GetNativeHandle()));
            CHECK(glTexSubImage2D(GL_TEXTURE_2D,
                request.mipLevel,
                request.imageOffset.x, request.imageOffset.y,
                request.imageExtent.width, request.imageExtent.height,
                fmt.format,
                fmt.type,
                reinterpret_cast<const GLvoid *>(request.data + request.offset)));
            CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        });
        return handle;
    };

    rhi::TransferTaskHandle Queue::UploadBuffer(const rhi::BufferPtr &buffer, const rhi::BufferUploadRequest &request)
    {
        auto glesBuffer = std::static_pointer_cast<Buffer>(buffer);
        auto handle = CreateTask([glesBuffer, request]() {
            auto target = glesBuffer->GetGLTarget();

            CHECK(glBindBuffer(target, glesBuffer->GetNativeHandle()));
            CHECK(glBufferData(target, request.size, request.source->GetData(request.offset), glesBuffer->GetGLUsage()));
            CHECK(glBindBuffer(target, 0));
        });
        return handle;
    }

    void Queue::Present(const SurfacePtr &surface)
    {
        CreateTask([this, surface]() {
//            context->MakeCurrent(*surface);
//            eglSwapInterval(context->GetDisplay(), 0);
            eglSwapBuffers(context->GetDisplay(), surface->GetSurface());
            SKY_ASSERT(eglGetError() == EGL_SUCCESS);
        });
    }
}

