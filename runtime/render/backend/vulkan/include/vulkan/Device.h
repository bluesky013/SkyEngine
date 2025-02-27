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
#include <vulkan/QueryPool.h>
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

        std::string GetDeviceInfo() const override;
        void WaitIdle() const  override;

        VmaAllocator     GetAllocator() const;
        VkDevice         GetNativeHandle() const;
        VkPhysicalDevice GetGpuHandle() const;
        VkInstance       GetInstanceId() const;

        Instance &GetInstance() const { return instance; }

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
        const AccessInfo     &GetAccessInfo(const rhi::AccessFlags& flags);

        // features
        const VkPhysicalDeviceProperties &GetProperties() const;
        bool    GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryPropertyFlags flags, MemoryRequirement &requirement) const;
        bool    GetImageMemoryRequirements(VkImage image, VkMemoryPropertyFlags flags, MemoryRequirement &requirement) const;
        int32_t FindProperties(uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties) const;
        void FeatureQuery(rhi::MeshShaderProperties& properties) const override;

        // rhi
        rhi::Queue *GetQueue(rhi::QueueType type) const override;

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
        CREATE_DEV_OBJ(QueryPool)
        CREATE_DEV_OBJ_FUNC(Semaphore, Sema)

#ifdef SKY_ENABLE_XR
        CREATE_DEV_OBJ(XRSwapChain)
#endif

        const SamplerPtr &GetDefaultSampler() const { return defaultSampler; }

        // Special Device Object
        std::shared_ptr<rhi::CommandBuffer> CreateCommandBuffer(const rhi::CommandBuffer::Descriptor &desc) override
        {
            return std::static_pointer_cast<rhi::CommandBuffer>(static_cast<Queue *>(GetQueue(desc.queueType))->AllocateCommandBuffer({}));
        }

        // Desc Object
        CREATE_DESC_OBJ(VertexInput)

        uint32_t CheckPipelineStatisticFlags(const rhi::PipelineStatisticFlags &val, rhi::PipelineStatisticFlags &res) override;
    private:
        bool Init(const Descriptor &, bool enableDebug);
        void InitPropAndFeatureChain();
        void ValidateFeature(const rhi::DeviceFeature &feature, std::vector<const char*> &outExtensions);
        void UpdateDeviceLimits();
        void UpdateFormatFeatures();
        void UpdateConstants();

        bool FillMemoryRequirements(VkMemoryRequirements2               &requirements,
                                    const VkMemoryDedicatedRequirements &dedicated,
                                    VkMemoryPropertyFlags                flags,
                                    MemoryRequirement                   &out) const;

        void PrintSupportedExtensions() const;
        void SetupDefaultResources();
        friend class Instance;
        explicit Device(Instance &);
        Instance        &instance;
        VkPhysicalDevice phyDev;
        VkDevice         device;
        VmaAllocator     allocator;

        SamplerPtr defaultSampler;

        std::vector<VkExtensionProperties> supportedExtensions;
        std::vector<VkPhysicalDeviceFragmentShadingRateKHR> shadingRates;

        VkPhysicalDeviceProperties2                      phyProps         = {};
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR shadingRateProps = {};
        VkPhysicalDeviceDepthStencilResolvePropertiesKHR dsResolveProps   = {};
        VkPhysicalDeviceMeshShaderPropertiesEXT          meshShaderProps  = {};

        VkPhysicalDeviceFeatures2                      phyFeatures         = {};
        VkPhysicalDeviceDescriptorIndexingFeatures     phyIndexingFeatures = {};
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR shadingRateFeatures = {};
        VkPhysicalDeviceMultiviewFeatures              mvrFeature          = {};
        VkPhysicalDeviceMeshShaderFeaturesEXT          meshShaderFeatures  = {};

        VkPhysicalDeviceFeatures                       enabledPhyFeatures         = {};
        VkPhysicalDeviceDescriptorIndexingFeatures     enabledPhyIndexingFeatures = {};
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR enabledShadingRateFeatures = {};
        VkPhysicalDeviceMultiviewFeatures              enabledMvrFeature          = {};
        VkPhysicalDeviceMeshShaderFeaturesEXT          enabledMeshShaderFeatures  = {};

        VkPhysicalDeviceMemoryProperties2 memoryProperties = {};

        std::vector<VkQueueFamilyProperties> queueFamilies;
        std::vector<QueuePtr>                queues;
        Queue                               *graphicsQueue;
        Queue                               *computeQueue;
        Queue                               *transferQueue;

        CacheManager<VkSampler, uint32_t>             samplers;
        CacheManager<VkDescriptorSetLayout, uint32_t> setLayouts;
        CacheManager<VkPipelineLayout, uint32_t>      pipelineLayouts;
        CacheManager<VkPipeline, uint32_t>            pipelines;
        CacheManager<VkRenderPass, uint32_t>          renderPasses;
        CacheManager<AccessInfo, uint64_t>            accessInfos;
    };

    const std::vector<const char *> &GetDeviceExtensions();

} // namespace sky::vk
