#pragma once

#include <common/FrameTransientPool.h>

#include <VulkanFunctions.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <common/FrameContext.h>

namespace sky::aurora {
    class VulkanDevice;
    class VulkanDeviceFrameContext;

    struct VulkanRenderPassInheritance {
        VkRenderPass  renderPass  = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        uint32_t      subpass     = 0;

        VkCommandBufferInheritanceInfo GetInfo() const noexcept
        {
            VkCommandBufferInheritanceInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
            info.renderPass  = renderPass;
            info.framebuffer = framebuffer;
            info.subpass     = subpass;
            return info;
        }
    };
    struct VulkanRenderingInheritance {
        VkFormat              colorFormat = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples     = VK_SAMPLE_COUNT_1_BIT;
        uint32_t              viewMask    = 0;
        VkFormat              depthFormat = VK_FORMAT_UNDEFINED;

        // The native view borrows this immutable plan's format storage.
        VkCommandBufferInheritanceRenderingInfo GetInfo() const & noexcept
        {
            VkCommandBufferInheritanceRenderingInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO};
            info.viewMask                = viewMask;
            info.colorAttachmentCount    = colorFormat == VK_FORMAT_UNDEFINED ? 0 : 1;
            info.pColorAttachmentFormats = info.colorAttachmentCount ? &colorFormat : nullptr;
            info.rasterizationSamples    = samples;
            info.depthAttachmentFormat   = depthFormat;
            return info;
        }
        VkCommandBufferInheritanceRenderingInfo GetInfo() const && = delete;
    };
    struct VulkanRasterPassPlan {
        uint32_t                                                              logicalPass;
        uint32_t                                                              nativeScope;
        std::variant<VulkanRenderPassInheritance, VulkanRenderingInheritance> inheritance;
    };
    struct VulkanEncodeTask {
        uint32_t rasterPass;
        uint32_t firstDraw;
        uint32_t drawCount;
    };
    struct VulkanFragmentRange {
        uint32_t first = 0;
        uint32_t count = 0;
    };
    constexpr uint32_t VULKAN_DRAWS_PER_FRAGMENT = 32;

    class VulkanFrameEncoder {
    public:
        virtual ~VulkanFrameEncoder() = default;
        // Custom callbacks run on the coordinator and may fail while encoding.
        virtual void Encode(const VulkanDeviceFunctions &, VkCommandBuffer, CommandBuffer &, std::span<const VkCommandBuffer>) noexcept = 0;
        virtual void
        EncodeFragment(const VulkanDeviceFunctions &, VkCommandBuffer, const VulkanRasterPassPlan &, const VulkanEncodeTask &) const noexcept = 0;
        std::optional<FrameError> encodeError;
    };
    class VulkanCompiledFrame final : public BackendCompiledFrame {
    public:
        CompiledFrameIdentity GetIdentity() const noexcept override
        {
            return reservation.identity;
        }
        FrameResourceBindings                   resources;
        FramePlanLease                          reservation;
        std::shared_ptr<const FrozenFrameGraph> graph;
        std::vector<uint32_t>                   livePasses;
        std::vector<VulkanRasterPassPlan>       rasterPasses;
        // Bindings are frozen; fragment nodes are independent. The coordinator joins in encoder order.
        std::vector<VulkanEncodeTask>       encodeTasks;
        std::unique_ptr<VulkanFrameEncoder> encoder;
    };
    using VulkanCompileResult = std::variant<FrameError, FrameNoWork, std::unique_ptr<VulkanCompiledFrame>>;

    class VulkanFrameCompiler {
    public:
        virtual ~VulkanFrameCompiler() = default;
        virtual VulkanCompileResult
        Compile(std::shared_ptr<const FrozenFrameGraph>, VulkanDeviceFrameContext &, const DeviceFrameLease &) noexcept = 0;
    };
    class VulkanDynamicRenderingCompiler final : public VulkanFrameCompiler {
    public:
        explicit VulkanDynamicRenderingCompiler(VulkanDevice &device) : device(device)
        {
        }
        VulkanCompileResult Compile(std::shared_ptr<const FrozenFrameGraph>, VulkanDeviceFrameContext &, const DeviceFrameLease &) noexcept override;

    private:
        VulkanDevice &device;
    };
    class VulkanRenderPassCompiler final : public VulkanFrameCompiler {
    public:
        explicit VulkanRenderPassCompiler(VulkanDevice &device) : device(device)
        {
        }
        VulkanCompileResult Compile(std::shared_ptr<const FrozenFrameGraph>, VulkanDeviceFrameContext &, const DeviceFrameLease &) noexcept override;

    private:
        VulkanDevice &device;
    };
} // namespace sky::aurora
