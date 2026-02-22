//
// Created by blues on 2024/9/16.
//

#pragma once

#include <list>
#include <render/resource/Texture.h>
#include <render/resource/Buffer.h>

namespace sky {

    class RenderStreamManager {
    public:
        RenderStreamManager() = default;
        ~RenderStreamManager() = default;

        void SetUploadQueue(rhi::Queue *queue);
        void UploadTexture(const RDTexturePtr &texture);
        void UploadBuffer(const RDBufferPtr &buffer);
        void Tick();

    private:
        std::list<RDStreamableResourcePtr> uploadQueue;

        rhi::Queue* transferQueue = nullptr;

        uint32_t limitPerFrame = 256 * 1024 * 1024;
    };

} // namespace sky
