//
// Aurora Vulkan Queue.
//

#pragma once

#include <aurora/rhi/Queue.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Fence.h>
#include <VulkanFunctions.h>

#include <memory>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;
    class CommandPool;

    class VulkanQueue : public Queue {
    public:
        VulkanQueue(VulkanDevice &dev, QueueType type, VkQueue native, uint32_t familyIndex);
        ~VulkanQueue() override;

        void      Submit(const SubmitInfo &info) override;
        void      WaitIdle() override;
        QueueType GetType() const override { return type; }

        TransferTaskHandle UploadBuffer(Buffer *buffer, const std::vector<BufferUploadRequest> &requests) override;
        TransferTaskHandle UploadImage(Image *image, const std::vector<ImageUploadRequest> &requests) override;

        void Wait(TransferTaskHandle handle) override;
        bool HasComplete(TransferTaskHandle handle) const override;

        VkQueue  GetNativeHandle() const { return queue; }
        uint32_t GetFamilyIndex() const { return familyIndex; }

    private:
        struct PendingUpload {
            FencePtr                     fence;
            BufferPtr                    staging;
            std::unique_ptr<CommandPool> pool;
        };

        VulkanDevice &device;
        QueueType     type;
        VkQueue       queue       = VK_NULL_HANDLE;
        uint32_t      familyIndex = 0;

        std::vector<PendingUpload> pendingUploads;
    };

} // namespace sky::aurora
