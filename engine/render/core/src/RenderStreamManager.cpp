//
// Created by blues on 2024/9/16.
//

#include <render/RenderStreamManager.h>

namespace sky {

    void RenderStreamManager::SetUploadQueue(rhi::Queue *queue)
    {
        transferQueue = queue;
    }

    void RenderStreamManager::UploadTexture(const RDTexturePtr &texture)
    {
        uploadQueue.emplace_back(texture.Get());
    }

    void RenderStreamManager::UploadBuffer(const RDBufferPtr &buffer)
    {
        uploadQueue.emplace_back(buffer.Get());
    }

    void RenderStreamManager::Tick()
    {
        uint64_t current = 0;
        while (!uploadQueue.empty()) {
            auto &res = uploadQueue.front();
            current += res->Upload(transferQueue);
            uploadQueue.pop_front();

            if (current >= limitPerFrame) {
                break;
            }
        }
    }

} // namespace sky