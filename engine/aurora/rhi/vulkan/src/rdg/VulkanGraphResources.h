#pragma once

#include "VulkanFrameCompiler.h"
#include <VulkanBuffer.h>
#include <VulkanImage.h>
#include <VulkanPipelineState.h>
#include <aurora/rdg/FrameBindings.h>
#include <map>
#include <optional>
#include <tuple>

namespace sky::aurora {
    class VulkanShader;
    struct VulkanResourceAccess {
        VkPipelineStageFlags2 stage  = 0;
        VkAccessFlags2        access = 0;
    };
    struct VulkanBarriers {
        std::vector<VkBufferMemoryBarrier2> buffers;
        std::vector<VkImageMemoryBarrier2>  images;
        void                                Encode(const VulkanDeviceFunctions &, VkCommandBuffer) const noexcept;
    };
    struct VulkanTransferStep {
        VulkanBarriers    before;
        VkBuffer          source      = VK_NULL_HANDLE;
        VkBuffer          destination = VK_NULL_HANDLE;
        VkImage           image       = VK_NULL_HANDLE;
        VkBufferCopy      bufferRegion{};
        VkBufferImageCopy imageRegion{};
        void              Encode(const VulkanDeviceFunctions &, VkCommandBuffer) const noexcept;
    };
    struct VulkanDescriptorSet {
        uint32_t              set        = 0;
        VkDescriptorSet       descriptor = VK_NULL_HANDLE;
        std::vector<uint32_t> offsets;
    };
    struct VulkanGraphImageView {
        explicit VulkanGraphImageView(VulkanDevice &device) : device(device)
        {
        }
        ~VulkanGraphImageView();
        VulkanDevice &device;
        VkImageView   view = VK_NULL_HANDLE;
    };
    struct VulkanFrameBindings {
        std::vector<VulkanDescriptorSet>                   sets;
        std::vector<std::shared_ptr<VulkanGraphImageView>> views;
    };
    struct VulkanDraw {
        CounterPtr<VulkanGraphicsPipeline>                 pipeline;
        CmdDrawLinear                                      command;
        VkRect2D                                           scissor{};
        std::vector<VkBuffer>                              vertices;
        std::vector<VkDeviceSize>                          vertexOffsets;
        VkBuffer                                           index       = VK_NULL_HANDLE;
        VkDeviceSize                                       indexOffset = 0;
        VkIndexType                                        indexType   = VK_INDEX_TYPE_UINT32;
        CmdDrawIndexed                                     indexed;
        VkPipelineLayout                                   layout = VK_NULL_HANDLE;
        std::vector<VulkanDescriptorSet>                   sets;
        std::vector<std::shared_ptr<VulkanGraphImageView>> views;
    };
    struct VulkanComputeStep {
        VulkanBarriers                                     before;
        CounterPtr<VulkanComputePipeline>                  pipeline;
        VkPipelineLayout                                   layout = VK_NULL_HANDLE;
        std::vector<VulkanDescriptorSet>                   sets;
        std::vector<std::shared_ptr<VulkanGraphImageView>> views;
        uint32_t                                           groupsX = 1, groupsY = 1, groupsZ = 1;
        void                                               Encode(const VulkanDeviceFunctions &, VkCommandBuffer) const noexcept;
    };
    struct VulkanCustomStep {
        VulkanBarriers                                before;
        std::shared_ptr<const FrameCustomPassEncoder> encoder;
        std::vector<Image *>                          images;
        std::vector<Buffer *>                         buffers;
        FrameOperationResult                          Encode(const VulkanDeviceFunctions &, VkCommandBuffer, CommandBuffer &) const noexcept;
    };
    class VulkanGraphResources;
    void EncodeVulkanDraws(const VulkanDeviceFunctions &, VkCommandBuffer, VkExtent2D, std::span<const VulkanDraw>) noexcept;
    std::variant<FrameError, std::vector<VulkanDraw>> ResolveVulkanDraws(VulkanDevice &,
                                                                         VulkanDeviceFrameContext &,
                                                                         const DeviceFrameLease &,
                                                                         VulkanGraphResources &,
                                                                         const FrameRasterPass &,
                                                                         VkExtent2D,
                                                                         PixelFormat,
                                                                         const SubpassInfo *);
    std::variant<FrameError, VulkanComputeStep>       ResolveVulkanCompute(VulkanDevice &,
                                                                           VulkanDeviceFrameContext &,
                                                                           const DeviceFrameLease &,
                                                                           VulkanGraphResources &,
                                                                           const FrameComputePass &,
                                                                           const FramePassResourceUses &);
    FrameError                                        VulkanFrameError(VkResult, FrameErrorCode) noexcept;

    // Allocation helpers and access tracking; each compiler decides scopes, transitions and command order.
    class VulkanGraphResources {
    public:
        VulkanGraphResources(VulkanDevice &device, const FrameGraphData &data, uint64_t epoch) : epoch(epoch), device(device), data(data)
        {
        }
        std::optional<FrameError>                    Resolve(const FrameGraphAnalysis &);
        std::variant<FrameError, VulkanTransferStep> Copy(const BufferCopyPass &, const FramePassResourceUses &);
        std::variant<FrameError, VulkanTransferStep> Copy(const BufferImageCopyPass &, const FramePassResourceUses &);
        std::variant<FrameError, VulkanCustomStep>   Custom(const FrameCustomPass &, const FramePassResourceUses &);
        std::optional<FrameError>                    RasterInputs(const FramePassResourceUses &, VulkanBarriers &);
        std::optional<FrameError>                    ShaderInputs(const FramePassResourceUses &, VulkanBarriers &);
        void                                         TransferInputs(const FramePassResourceUses &, VulkanBarriers &);
        std::variant<FrameError, VulkanFrameBindings>
        ResolveBindings(VulkanDeviceFrameContext &, const DeviceFrameLease &, VulkanShader &, const FrameBindingGroups &);
        std::variant<FrameError, RasterImageView> ResolveDeclaredView(const FrameImageBinding &, bool storage = false) const;
        // The range must already be normalized and validated against this image.
        std::variant<FrameError, std::shared_ptr<VulkanGraphImageView>> ImageView(uint32_t, const RasterImageView &);
        std::variant<FrameError, std::shared_ptr<VulkanGraphImageView>> AttachmentView(GraphImageHandle, const RasterAttachmentView &);
        void        ImageBarrier(uint32_t, ImageLayout target, VulkanBarriers &, const ImageSubRange &range = {});
        void        BufferBarrier(uint32_t, CopyBufferState target, VulkanBarriers &, VkPipelineStageFlags2 stages = 0, VkAccessFlags2 access = 0);
        void        Finish(const FrameGraphAnalysis &, VulkanBarriers &);
        ImageLayout GetImageState(uint32_t index, uint32_t subresource) const
        {
            return imageStates[bindings.imageBacking[index]][subresource];
        }
        void SetImageState(uint32_t index, uint32_t subresource, ImageLayout state)
        {
            imageStates[bindings.imageBacking[index]][subresource] = state;
        }
        FrameResourceBindings       bindings;
        std::vector<VulkanBuffer *> buffers;
        std::vector<VulkanImage *>  images;

    private:
        uint64_t                              epoch;
        VulkanDevice                         &device;
        const FrameGraphData                 &data;
        std::vector<VulkanResourceAccess>     bufferStates;
        std::vector<std::vector<ImageLayout>> imageStates;
        using ViewKey = std::tuple<uint32_t, ImageViewType, uint32_t, uint32_t, uint32_t, uint32_t>;
        std::map<ViewKey, std::shared_ptr<VulkanGraphImageView>>                        imageViews;
        std::map<const FrameBindingGroup *, VulkanFrameBindings, FrameBindingGroupLess> bindingGroups;
    };
} // namespace sky::aurora
