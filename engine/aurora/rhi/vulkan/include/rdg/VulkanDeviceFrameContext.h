#pragma once

#include <common/FrameContext.h>
#include <core/platform/Platform.h>
#include <vulkan/vulkan.h>

#ifdef Aurora_Vulkan_EXPORTS
    #define AURORA_VULKAN_FRAME_API SKY_EXPORT
#else
    #define AURORA_VULKAN_FRAME_API SKY_IMPORT
#endif

namespace sky::aurora {
    class VulkanDevice;
    class Shader;
    class VulkanFrameCompiler;

    class VulkanDeviceFrameContext final : public BackendFrameContext {
    public:
        explicit VulkanDeviceFrameContext(VulkanDevice *device);
        ~VulkanDeviceFrameContext() override;
        FrameCompileResult   Compile(const DeviceFrameLease &, std::shared_ptr<const FrozenFrameGraph>) override;
        FrameDispatchResult  Dispatch(std::unique_ptr<BackendCompiledFrame> &) override;
        FrameCollectResult   TryCollect(const EncodingTicket &) override;
        FrameOperationResult Cancel(const EncodingTicket &) override;

        AURORA_VULKAN_FRAME_API std::variant<FrameError, VkDescriptorSet>
                                AllocateDescriptorSet(const DeviceFrameLease &, const Shader &, uint32_t set);

    private:
        FrameOperationResult                 ResetLeaseResources(uint32_t) noexcept override;
        FrameBufferStorageResult             CreateFrameBuffer(const Buffer::Descriptor &) override;
        bool                                 InitBackend(const DeviceFrameContextInitInfo &) override;
        VulkanDevice                        *device;
        std::unique_ptr<VulkanFrameCompiler> compiler;
        struct State;
        struct EncodingSession;
        std::unique_ptr<State> state;
        FrameRecordResult      RecordPrimary(EncodingSession &);
    };

} // namespace sky::aurora
