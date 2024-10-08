//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/Queue.h>
#include <gles/DevObject.h>
#include <gles/PipelineStateCache.h>
#include <gles/egl/Context.h>

namespace sky::gles {
    class Device;

    class Queue : public rhi::Queue, public DevObject {
    public:
        Queue(Device &dev) : DevObject(dev) {}
        ~Queue() = default;

        bool Init(const Context::Descriptor &cfg, rhi::QueueType type);
        Context *GetContext() const { return context.get(); }
        PipelineCacheState *GetCacheState() const { return state.get(); }

        rhi::TransferTaskHandle UploadImage(const rhi::ImagePtr &image, const std::vector<rhi::ImageUploadRequest> &requests) override;
        rhi::TransferTaskHandle UploadBuffer(const rhi::BufferPtr &image, const std::vector<rhi::BufferUploadRequest> &requests) override;

        void Present(const SurfacePtr &surface);

    private:
        void PreShutdown() override;

        void GenerateBlitFrameBuffer();
        void DestroyBlitFrameBuffer();

        std::unique_ptr<Context> context;
        std::unique_ptr<PipelineCacheState> state;
        GLuint blitFrameBuffer = 0;
    };

}
