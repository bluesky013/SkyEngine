//
// Created by Zach Lee on 2021/11/7.
//

#pragma once
#include <core/template/ReferenceObject.h>
#include <vector>
#include <vk_mem_alloc.h>
#include <rhi/Device.h>
#include <vulkan/CacheManager.h>
#include <vulkan/Queue.h>
#include <vulkan/Swapchain.h>
#include <vulkan/RenderPass.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/CommandBuffer.h>
#include <vulkan/Fence.h>
#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/VertexInput.h>
#include <vulkan/Semaphore.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/vulkan.h>

namespace sky::vk {

    class Instance;

    class Device : public rhi::Device {
    public:
        ~Device() override;

        template <typename T, typename Desc>
        inline std::shared_ptr<T> CreateDeviceObject(const Desc &des)
        {
            auto res = new T(*this);
            if (!res->Init(des)) {
                delete res;
                res = nullptr;
            }
            return std::shared_ptr<T>(res);
        }

        template <typename T, typename Desc>
        inline std::shared_ptr<T> CreateDescObject(const Desc &des)
        {
            auto res = std::make_shared<T>();
            if (!res->Init(des)) {
                res = nullptr;
            }
            return res;
        }

        void WaitIdle() const  override;

        VmaAllocator     GetAllocator() const;
        VkDevice         GetNativeHandle() const;
        VkPhysicalDevice GetGpuHandle() const;
        VkInstance       GetInstance() const;

        // Queue
        Queue *GetQueue(VkQueueFlags preferred, VkQueueFlags excluded) const;
        Queue *GetGraphicsQueue() const;
        Queue *GetTransferQueue() const;

        // Cache object
        VkSampler             GetSampler(uint32_t hash, VkSamplerCreateInfo *samplerInfo = nullptr);
        VkPipelineLayout      GetPipelineLayout(uint32_t hash, VkPipelineLayoutCreateInfo * = nullptr);
        VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t hash, VkDescriptorSetLayoutCreateInfo * = nullptr);
        VkRenderPass          GetRenderPass(uint32_t hash, VkRenderPassCreateInfo2 * = nullptr);
        VkPipeline            GetPipeline(uint32_t hash, VkGraphicsPipelineCreateInfo * = nullptr);

        // features
        const VkPhysicalDeviceProperties &GetProperties() const;
        bool    GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryPropertyFlags flags, MemoryRequirement &requirement) const;
        bool    GetImageMemoryRequirements(VkImage image, VkMemoryPropertyFlags flags, MemoryRequirement &requirement) const;
        int32_t FindProperties(uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties) const;

        // rhi
        rhi::Queue *GetQueue(rhi::QueueType type) const override
        {
            return graphicsQueue;
        }

        // Device Object
        CREATE_DEV_OBJ(SwapChain)
        CREATE_DEV_OBJ(Image)
        CREATE_DEV_OBJ(Buffer)
        CREATE_DEV_OBJ(RenderPass)
        CREATE_DEV_OBJ(FrameBuffer)
        CREATE_DEV_OBJ(Fence)
        CREATE_DEV_OBJ(Shader)
        CREATE_DEV_OBJ(GraphicsPipeline)
        CREATE_DEV_OBJ(DescriptorSetLayout)
        CREATE_DEV_OBJ(PipelineLayout)
        CREATE_DEV_OBJ(VertexAssembly)
        CREATE_DEV_OBJ(Sampler)
        CREATE_DEV_OBJ(DescriptorSetPool)
        CREATE_DEV_OBJ_FUNC(Semaphore, Sema)

        const SamplerPtr &GetDefaultSampler() const { return defaultSampler; }

        // Special Device Object
        std::shared_ptr<rhi::CommandBuffer> CreateCommandBuffer(const rhi::CommandBuffer::Descriptor &desc) override
        {
            return std::static_pointer_cast<rhi::CommandBuffer>(static_cast<Queue *>(GetQueue(desc.queueType))->AllocateCommandBuffer({}));
        }

        // Desc Object
        CREATE_DESC_OBJ(VertexInput)
    private:
        bool Init(const Descriptor &, bool enableDebug);

        void ValidateFeature(const DeviceFeature &feature, std::vector<const char*> &outExtensions);
        void UpdateDeviceLimits();

        bool FillMemoryRequirements(VkMemoryRequirements2               &requirements,
                                    const VkMemoryDedicatedRequirements &dedicated,
                                    VkMemoryPropertyFlags                flags,
                                    MemoryRequirement                   &out) const;

        void PrintSupportedExtensions() const;
        void SetupDefaultResources();

        friend class Instance;
        Device(Instance &);
        Instance        &instance;
        VkPhysicalDevice phyDev;
        VkDevice         device;
        VmaAllocator     allocator;

        SamplerPtr defaultSampler;

        std::vector<VkExtensionProperties> supportedExtensions;
        std::vector<VkPhysicalDeviceFragmentShadingRateKHR> shadingRates;

        VkPhysicalDeviceProperties2                      phyProps         = {};
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR shadingRateProps = {};

        VkPhysicalDeviceFeatures2                      phyFeatures         = {};
        VkPhysicalDeviceDescriptorIndexingFeatures     phyIndexingFeatures = {};
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR shadingRateFeatures = {};
        VkPhysicalDeviceMultiviewFeatures              mvrFeature          = {};

        VkPhysicalDeviceFeatures                       enabledPhyFeatures         = {};
        VkPhysicalDeviceDescriptorIndexingFeatures     enabledPhyIndexingFeatures = {};
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR enabledShadingRateFeatures = {};
        VkPhysicalDeviceMultiviewFeatures              enabledMvrFeature          = {};

        VkPhysicalDeviceMemoryProperties2 memoryProperties = {};

        std::vector<VkQueueFamilyProperties> queueFamilies;
        std::vector<QueuePtr>                queues;
        Queue                               *graphicsQueue;
        Queue                               *transferQueue;

        CacheManager<VkSampler>             samplers;
        CacheManager<VkDescriptorSetLayout> setLayouts;
        CacheManager<VkPipelineLayout>      pipelineLayouts;
        CacheManager<VkPipeline>            pipelines;
        CacheManager<VkRenderPass>          renderPasses;
    };

} // namespace sky::vk
