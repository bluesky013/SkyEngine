#include "rdg/VulkanFrameCompiler.h"
#include "GpuTestSupport.h"
#include "graph/FrameGraphBuilder.h"
#include "rdg/VulkanRenderPassCache.h"
#include <algorithm>
#include <atomic>
#include <aurora/rdg/DeviceFrameContext.h>
#include <condition_variable>
#include <future>
#include <map>
#include <set>

namespace {
    void EvictFrameCaches(VulkanDevice &device)
    {
        device.GetFramePipelineCache().Clear();
        device.GetRenderPassCache().Clear();
    }
    struct NativeProbe {
        explicit NativeProbe(VulkanDevice &device) : fn(const_cast<VulkanDeviceFunctions &>(device.GetDeviceFn())), saved(fn)
        {
            EvictFrameCaches(device);
            active                       = this;
            fn.vkCmdBeginRendering       = BeginDynamic;
            fn.vkCmdEndRendering         = EndDynamic;
            fn.vkCmdBeginRenderPass2     = BeginRenderPass;
            fn.vkCmdNextSubpass2         = NextSubpass;
            fn.vkCmdEndRenderPass2       = EndRenderPass;
            fn.vkCmdPipelineBarrier2     = Barrier;
            fn.vkCreateRenderPass2       = CreateRenderPass;
            fn.vkCreateGraphicsPipelines = CreatePipelines;
            fn.vkDestroyPipeline         = DestroyPipeline;
            fn.vkCreateFramebuffer       = CreateFramebuffer;
            fn.vkDestroyFramebuffer      = DestroyFramebuffer;
            fn.vkDestroyRenderPass       = DestroyRenderPass;
        }
        ~NativeProbe()
        {
            fn     = saved;
            active = nullptr;
        }
        static void VKAPI_PTR BeginDynamic(VkCommandBuffer command, const VkRenderingInfo *info)
        {
            EXPECT_EQ(std::this_thread::get_id(), active->coordinator);
            EXPECT_FALSE(active->insideScope);
            active->insideScope = true;
            ++active->dynamicBegins;
            active->saved.vkCmdBeginRendering(command, info);
        }
        static void VKAPI_PTR BeginRenderPass(VkCommandBuffer command, const VkRenderPassBeginInfo *info, const VkSubpassBeginInfo *subpass)
        {
            EXPECT_EQ(std::this_thread::get_id(), active->coordinator);
            EXPECT_FALSE(active->insideScope);
            active->insideScope = true;
            ++active->renderPassBegins;
            active->saved.vkCmdBeginRenderPass2(command, info, subpass);
        }
        static void VKAPI_PTR EndDynamic(VkCommandBuffer command)
        {
            EXPECT_TRUE(active->insideScope);
            active->insideScope = false;
            active->saved.vkCmdEndRendering(command);
        }
        static void VKAPI_PTR NextSubpass(VkCommandBuffer command, const VkSubpassBeginInfo *begin, const VkSubpassEndInfo *end)
        {
            EXPECT_TRUE(active->insideScope);
            ++active->subpassTransitions;
            active->saved.vkCmdNextSubpass2(command, begin, end);
        }
        static void VKAPI_PTR EndRenderPass(VkCommandBuffer command, const VkSubpassEndInfo *end)
        {
            EXPECT_TRUE(active->insideScope);
            active->insideScope = false;
            active->saved.vkCmdEndRenderPass2(command, end);
        }
        static void VKAPI_PTR Barrier(VkCommandBuffer command, const VkDependencyInfo *info)
        {
            EXPECT_EQ(std::this_thread::get_id(), active->coordinator);
            EXPECT_FALSE(active->insideScope);
            if (active->inspectStorageStages) {
                auto check = [&](VkAccessFlags2 access, VkPipelineStageFlags2 stages, uint32_t &count) {
                    if (!(access & (VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)))
                        return;
                    ++count;
                    EXPECT_TRUE((stages & VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) ||
                                ((stages & VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT) &&
                                 (stages & (VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT))));
                };
                for (uint32_t i = 0; i < info->bufferMemoryBarrierCount; ++i) {
                    const auto &barrier = info->pBufferMemoryBarriers[i];
                    check(barrier.srcAccessMask, barrier.srcStageMask, active->storageSources);
                    check(barrier.dstAccessMask, barrier.dstStageMask, active->storageDestinations);
                }
                for (uint32_t i = 0; i < info->imageMemoryBarrierCount; ++i) {
                    const auto &barrier = info->pImageMemoryBarriers[i];
                    check(barrier.srcAccessMask, barrier.srcStageMask, active->storageSources);
                    check(barrier.dstAccessMask, barrier.dstStageMask, active->storageDestinations);
                }
            }
            active->saved.vkCmdPipelineBarrier2(command, info);
        }
        static VkResult VKAPI_PTR CreateRenderPass(VkDevice                       device,
                                                   const VkRenderPassCreateInfo2 *info,
                                                   const VkAllocationCallbacks   *allocator,
                                                   VkRenderPass                  *output)
        {
            active->subpassCounts.push_back(info->subpassCount);
            active->scopeLoads.push_back(info->pAttachments[0].loadOp);
            active->scopeStores.push_back(info->pAttachments[0].storeOp);
            EXPECT_EQ(info->dependencyCount, info->subpassCount + 1);
            for (uint32_t i = 1; i < info->subpassCount; ++i) {
                const auto &dependency = info->pDependencies[i];
                EXPECT_EQ(dependency.srcSubpass, i - 1);
                EXPECT_EQ(dependency.dstSubpass, i);
                EXPECT_TRUE(dependency.srcAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
                EXPECT_TRUE(dependency.dstAccessMask & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
            }
            return active->saved.vkCreateRenderPass2(device, info, allocator, output);
        }
        static VkResult VKAPI_PTR CreatePipelines(VkDevice                            device,
                                                  VkPipelineCache                     cache,
                                                  uint32_t                            count,
                                                  const VkGraphicsPipelineCreateInfo *infos,
                                                  const VkAllocationCallbacks        *allocator,
                                                  VkPipeline                         *output)
        {
            ++active->pipelineAttempts;
            const bool fail =
                active->pipelineFailure != VK_SUCCESS && (!active->failPipelineAttempt || active->pipelineAttempts == active->failPipelineAttempt);
            for (uint32_t i = 0; i < count; ++i) {
                if (infos[i].renderPass) {
                    ++active->renderPassPipelines;
                    active->pipelineSubpasses.push_back(infos[i].subpass);
                    EXPECT_EQ(infos[i].pNext, nullptr);
                } else {
                    ++active->dynamicPipelines;
                    CheckRenderingInfo(infos[i].pNext);
                }
                if (fail)
                    output[i] = VK_NULL_HANDLE;
            }
            if (fail)
                return active->pipelineFailure;
            return active->saved.vkCreateGraphicsPipelines(device, cache, count, infos, allocator, output);
        }
        static void CheckRenderingInfo(const void *next)
        {
            ASSERT_NE(next, nullptr);
            EXPECT_EQ(static_cast<const VkBaseInStructure *>(next)->sType, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
        }
        static void VKAPI_PTR DestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *allocator)
        {
            ++active->pipelinesDestroyed;
            active->saved.vkDestroyPipeline(device, pipeline, allocator);
        }
        static VkResult VKAPI_PTR CreateFramebuffer(VkDevice                       device,
                                                    const VkFramebufferCreateInfo *info,
                                                    const VkAllocationCallbacks   *allocator,
                                                    VkFramebuffer                 *output)
        {
            if (active->framebufferFailure != VK_SUCCESS) {
                *output = VK_NULL_HANDLE;
                return active->framebufferFailure;
            }
            const auto result = active->saved.vkCreateFramebuffer(device, info, allocator, output);
            if (result == VK_SUCCESS)
                active->framebuffers.emplace_back(info->renderPass, *output);
            return result;
        }
        static void VKAPI_PTR DestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *allocator)
        {
            ++active->framebuffersDestroyed;
            active->saved.vkDestroyFramebuffer(device, framebuffer, allocator);
        }
        static void VKAPI_PTR DestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *allocator)
        {
            ++active->renderPassesDestroyed;
            active->saved.vkDestroyRenderPass(device, renderPass, allocator);
        }
        VulkanDeviceFunctions                              &fn;
        VulkanDeviceFunctions                               saved;
        const std::thread::id                               coordinator   = std::this_thread::get_id();
        uint32_t                                            dynamicBegins = 0, renderPassBegins = 0, dynamicPipelines = 0, renderPassPipelines = 0;
        uint32_t                                            pipelinesDestroyed = 0, renderPassesDestroyed = 0, framebuffersDestroyed = 0;
        VkResult                                            pipelineFailure = VK_SUCCESS, framebufferFailure = VK_SUCCESS;
        bool                                                insideScope          = false;
        bool                                                inspectStorageStages = false;
        uint32_t                                            storageSources = 0, storageDestinations = 0;
        uint32_t                                            subpassTransitions = 0, pipelineAttempts = 0, failPipelineAttempt = 0;
        std::vector<uint32_t>                               subpassCounts, pipelineSubpasses;
        std::vector<VkAttachmentLoadOp>                     scopeLoads;
        std::vector<VkAttachmentStoreOp>                    scopeStores;
        std::vector<std::pair<VkRenderPass, VkFramebuffer>> framebuffers;
        static inline NativeProbe                          *active = nullptr;
    };

    struct RecordingProbe {
        enum class Fault { NONE, WORKER_RESET, WORKER_ALLOCATE, WORKER_BEGIN, PRIMARY_BEGIN, PRIMARY_END };
        explicit RecordingProbe(VulkanDevice &device) : fn(const_cast<VulkanDeviceFunctions &>(device.GetDeviceFn())), saved(fn)
        {
            active                      = this;
            fn.vkAllocateCommandBuffers = Allocate;
            fn.vkResetCommandPool       = Reset;
            fn.vkBeginCommandBuffer     = Begin;
            fn.vkEndCommandBuffer       = End;
            fn.vkCmdDraw                = Draw;
            fn.vkCmdExecuteCommands     = Execute;
            fn.vkDestroyCommandPool     = DestroyPool;
            fn.vkQueueWaitIdle          = WaitIdle;
            fn.vkQueueSubmit2           = Submit;
            fn.vkResetFences            = ResetFences;
            fn.vkGetFenceStatus         = Query;
        }
        ~RecordingProbe()
        {
            fn     = saved;
            active = nullptr;
        }
        struct Command {
            VkCommandPool pool      = VK_NULL_HANDLE;
            uint32_t      first     = UINT32_MAX;
            bool          recording = false;
        };
        static VkResult VKAPI_PTR Allocate(VkDevice device, const VkCommandBufferAllocateInfo *info, VkCommandBuffer *output)
        {
            if (info->level == VK_COMMAND_BUFFER_LEVEL_SECONDARY && active->Fail(Fault::WORKER_ALLOCATE))
                return active->faultResult;
            const auto result = active->saved.vkAllocateCommandBuffers(device, info, output);
            if (result == VK_SUCCESS && info->level == VK_COMMAND_BUFFER_LEVEL_SECONDARY) {
                std::lock_guard lock(active->mutex);
                for (uint32_t i = 0; i < info->commandBufferCount; ++i)
                    active->commands[output[i]].pool = info->commandPool;
            }
            return result;
        }
        static VkResult VKAPI_PTR Reset(VkDevice device, VkCommandPool pool, VkCommandPoolResetFlags flags)
        {
            if (std::this_thread::get_id() != active->coordinator && active->Fail(Fault::WORKER_RESET))
                return active->faultResult;
            {
                std::lock_guard lock(active->mutex);
                EXPECT_FALSE(active->protectedPools.contains(pool));
                for (const auto &[command, info] : active->commands)
                    if (info.pool == pool)
                        EXPECT_FALSE(info.recording);
            }
            return active->saved.vkResetCommandPool(device, pool, flags);
        }
        static VkResult VKAPI_PTR Begin(VkCommandBuffer command, const VkCommandBufferBeginInfo *info)
        {
            if (info->flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) {
                if (active->Fail(Fault::WORKER_BEGIN))
                    return active->faultResult;
                std::unique_lock lock(active->mutex);
                auto            &entry = active->commands.at(command);
                EXPECT_FALSE(entry.recording);
                entry.recording   = true;
                entry.first       = UINT32_MAX;
                const auto thread = std::this_thread::get_id();
                EXPECT_NE(thread, active->coordinator);
                auto [owner, inserted] = active->poolOwners.emplace(entry.pool, thread);
                EXPECT_TRUE(inserted || owner->second == thread);
                active->threads.insert(thread);
                ++active->arrivals;
                active->maxConcurrent = std::max(active->maxConcurrent, ++active->concurrent);
                EXPECT_NE(info->pInheritanceInfo, nullptr);
                const auto &inheritance = *info->pInheritanceInfo;
                if (inheritance.renderPass) {
                    EXPECT_NE(inheritance.framebuffer, VK_NULL_HANDLE);
                    EXPECT_EQ(inheritance.pNext, nullptr);
                } else {
                    const auto *rendering = static_cast<const VkCommandBufferInheritanceRenderingInfo *>(inheritance.pNext);
                    EXPECT_NE(rendering, nullptr);
                    if (rendering) {
                        EXPECT_EQ(rendering->sType, VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO);
                        EXPECT_EQ(rendering->flags, 0u);
                        EXPECT_EQ(rendering->colorAttachmentCount, 1u);
                        EXPECT_EQ(*rendering->pColorAttachmentFormats, VK_FORMAT_R8G8B8A8_UNORM);
                    }
                }
                active->changed.notify_all();
                if (active->gateBegin && active->arrivals <= 2)
                    EXPECT_TRUE(active->changed.wait_for(lock, 5s, [] { return active->arrivals >= 2; }));
            } else {
                EXPECT_EQ(std::this_thread::get_id(), active->coordinator);
                ++active->primaryBegins;
                if (active->Fail(Fault::PRIMARY_BEGIN))
                    return active->faultResult;
            }
            return active->saved.vkBeginCommandBuffer(command, info);
        }
        static void VKAPI_PTR Draw(VkCommandBuffer command, uint32_t vertices, uint32_t instances, uint32_t firstVertex, uint32_t firstInstance)
        {
            {
                std::unique_lock lock(active->mutex);
                if (auto found = active->commands.find(command); found != active->commands.end()) {
                    if (found->second.first == UINT32_MAX)
                        found->second.first = firstInstance;
                    if (active->reverseFirst && firstInstance == 0)
                        EXPECT_TRUE(active->changed.wait_for(lock, 5s, [] { return active->finished > 0; }));
                }
            }
            active->saved.vkCmdDraw(command, vertices, instances, firstVertex, firstInstance);
        }
        static VkResult VKAPI_PTR End(VkCommandBuffer command)
        {
            std::unique_lock lock(active->mutex);
            auto             found = active->commands.find(command);
            if (found == active->commands.end()) {
                if (active->Fail(Fault::PRIMARY_END))
                    return active->faultResult;
                return active->saved.vkEndCommandBuffer(command);
            }
            const auto first = found->second.first;
            if (active->holdEnd && first != 0)
                EXPECT_TRUE(active->changed.wait_for(lock, 5s, [] { return active->release; }));
            found->second.recording = false;
            --active->concurrent;
            ++active->finished;
            active->completedOrder.push_back(first);
            active->changed.notify_all();
            if (auto error = active->endFailures.find(first); error != active->endFailures.end())
                return error->second;
            if (active->failFirstEnd && first == 0)
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            return active->saved.vkEndCommandBuffer(command);
        }
        static void VKAPI_PTR Execute(VkCommandBuffer command, uint32_t count, const VkCommandBuffer *secondary)
        {
            EXPECT_EQ(std::this_thread::get_id(), active->coordinator);
            {
                std::lock_guard lock(active->mutex);
                for (uint32_t i = 0; i < count; ++i) {
                    const auto &entry = active->commands.at(secondary[i]);
                    EXPECT_FALSE(entry.recording);
                    active->executedOrder.push_back(entry.first);
                    active->executedCommands.push_back(secondary[i]);
                }
            }
            active->saved.vkCmdExecuteCommands(command, count, secondary);
        }
        void Release()
        {
            std::lock_guard lock(mutex);
            release = true;
            changed.notify_all();
        }
        bool Fail(Fault point)
        {
            return fault == point && !injected.exchange(true, std::memory_order_relaxed);
        }
        static void VKAPI_PTR DestroyPool(VkDevice device, VkCommandPool pool, const VkAllocationCallbacks *allocator)
        {
            {
                std::lock_guard lock(active->mutex);
                EXPECT_EQ(active->concurrent, 0u);
                if (active->requireGpuWait)
                    EXPECT_TRUE(active->idleComplete);
                ++active->poolsDestroyed;
            }
            active->saved.vkDestroyCommandPool(device, pool, allocator);
        }
        static VkResult VKAPI_PTR WaitIdle(VkQueue queue)
        {
            {
                std::unique_lock lock(active->mutex);
                EXPECT_EQ(active->concurrent, 0u);
                active->idleEntered = true;
                active->changed.notify_all();
                if (active->holdIdle)
                    EXPECT_TRUE(active->changed.wait_for(lock, 5s, [] { return active->releaseIdle; }));
            }
            const auto      result = active->saved.vkQueueWaitIdle(queue);
            std::lock_guard lock(active->mutex);
            active->idleComplete = true;
            return result;
        }
        static VkResult VKAPI_PTR Submit(VkQueue queue, uint32_t count, const VkSubmitInfo2 *info, VkFence fence)
        {
            ++active->submitCalls;
            return active->submitResult == VK_SUCCESS ? active->saved.vkQueueSubmit2(queue, count, info, fence) : active->submitResult;
        }
        static VkResult VKAPI_PTR ResetFences(VkDevice device, uint32_t count, const VkFence *fences)
        {
            ++active->fenceResets;
            return active->fenceResetResult == VK_SUCCESS ? active->saved.vkResetFences(device, count, fences) : active->fenceResetResult;
        }
        static VkResult VKAPI_PTR Query(VkDevice device, VkFence fence)
        {
            return active->queryResult == VK_SUCCESS ? active->saved.vkGetFenceStatus(device, fence) : active->queryResult;
        }
        VulkanDeviceFunctions                   &fn;
        VulkanDeviceFunctions                    saved;
        const std::thread::id                    coordinator = std::this_thread::get_id();
        std::mutex                               mutex;
        std::condition_variable                  changed;
        std::map<VkCommandBuffer, Command>       commands;
        std::map<VkCommandPool, std::thread::id> poolOwners;
        std::set<std::thread::id>                threads;
        std::set<VkCommandPool>                  protectedPools;
        std::vector<uint32_t>                    completedOrder, executedOrder;
        std::vector<VkCommandBuffer>             executedCommands;
        uint32_t                                 arrivals = 0, finished = 0, concurrent = 0, maxConcurrent = 0, primaryBegins = 0;
        bool                                     gateBegin = false, reverseFirst = false, holdEnd = false, failFirstEnd = false, release = false;
        Fault                                    fault        = Fault::NONE;
        VkResult                                 faultResult  = VK_ERROR_OUT_OF_HOST_MEMORY;
        VkResult                                 submitResult = VK_SUCCESS, fenceResetResult = VK_SUCCESS, queryResult = VK_SUCCESS;
        std::atomic_bool                         injected{false};
        std::map<uint32_t, VkResult>             endFailures;
        uint32_t                                 poolsDestroyed = 0, submitCalls = 0, fenceResets = 0;
        bool                                     requireGpuWait = false, idleEntered = false, idleComplete = false, holdIdle = false;
        bool                                     releaseIdle = false;
        static inline RecordingProbe            *active      = nullptr;
    };

    FrameCollectResult AwaitEncoding(DeviceFrameContext &context, const EncodingTicket &ticket)
    {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        auto       result   = context.TryCollect(ticket);
        while (std::holds_alternative<FramePending>(result) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
            result = context.TryCollect(ticket);
        }
        return result;
    }

    class CustomComputeEncoder final : public FrameCustomPassEncoder {
    public:
        ComputePipeline     *pipeline = nullptr;
        ResourceGroup       *group    = nullptr;
        FrameOperationResult Encode(FrameCustomPassContext &context) const noexcept override
        {
            auto encoder = context.command.CreateComputeEncoder();
            encoder->BindPipeline(pipeline);
            encoder->BindResourceGroup(0, group);
            encoder->Dispatch(4, 1, 1);
            return FrameOperationComplete{};
        }
    };
    class VulkanFrameCompilerFixture : public testing::TestWithParam<VulkanFrameGraphMode> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"VulkanFrameCompilerTest", "SkyEngine", true, API::VULKAN});
            device = static_cast<VulkanDevice *>(Instance::Get()->GetDevice());
            ASSERT_TRUE(device);
            context.reset(device->CreateFrameContext({2, 1, 91, 2, GetParam()}));
            ASSERT_TRUE(context);
        }
        void TearDown() override
        {
            context.reset();
            Instance::Destroy();
        }
        ShaderPtr ShaderOf(bool green = false)
        {
            const auto vs = CompileGpuFunction(*device, kGpuFullscreenSource, ShaderStageFlagBit::VS, "VSMain");
            const auto fs = CompileGpuFunction(*device, kGpuFullscreenSource, ShaderStageFlagBit::FS, green ? "Green" : "Red");
            if (!vs || !fs)
                return {};
            Shader::Descriptor descriptor{};
            descriptor.vs = vs.Get();
            descriptor.ps = fs.Get();
            return ShaderPtr(device->CreateShader(descriptor));
        }
        FrameGraphImage ImageOf()
        {
            FrameGraphImage image;
            image.descriptor.format = PixelFormat::RGBA8_UNORM;
            image.descriptor.extent = {8, 8, 1};
            image.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC | ImageUsageFlagBit::TRANSFER_DST;
            image.image             = ImagePtr(device->CreateImage(image.descriptor));
            return image;
        }
        BufferPtr BufferOf(BufferUsageFlagBit usage)
        {
            Buffer::Descriptor desc;
            desc.size   = 256;
            desc.memory = MemoryType::CPU_ONLY;
            desc.usage  = usage;
            return BufferPtr(device->CreateBuffer(desc));
        }
        BufferPtr StorageBufferOf()
        {
            Buffer::Descriptor desc;
            desc.size   = 64;
            desc.memory = MemoryType::GPU_ONLY;
            desc.usage  = BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_SRC;
            return BufferPtr(device->CreateBuffer(desc));
        }
        ShaderPtr ComputeShaderOf()
        {
            const auto         cs = CompileGpuFunction(*device, kGpuComputeSource, ShaderStageFlagBit::CS, "CSMain");
            Shader::Descriptor descriptor{};
            descriptor.cs = cs.Get();
            return cs ? ShaderPtr(device->CreateShader(descriptor)) : ShaderPtr{};
        }
        FrameGraphImage StorageImageOf()
        {
            FrameGraphImage image;
            image.descriptor.format = PixelFormat::RGBA8_UNORM;
            image.descriptor.extent = {8, 8, 1};
            image.descriptor.usage  = ImageUsageFlagBit::STORAGE | ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_SRC;
            image.image             = ImagePtr(device->CreateImage(image.descriptor));
            return image;
        }
        ShaderPtr ComputeImageShaderOf()
        {
            const auto         cs = CompileGpuFunction(*device, kGpuComputeImageSource, ShaderStageFlagBit::CS, "CSMain");
            Shader::Descriptor descriptor{};
            descriptor.cs = cs.Get();
            return cs ? ShaderPtr(device->CreateShader(descriptor)) : ShaderPtr{};
        }
        ShaderPtr SampleShaderOf()
        {
            const auto vs = CompileGpuFunction(*device, kGpuSampleSource, ShaderStageFlagBit::VS, "VSMain");
            const auto fs = CompileGpuFunction(*device, kGpuSampleSource, ShaderStageFlagBit::FS, "PSMain");
            if (!vs || !fs)
                return {};
            Shader::Descriptor descriptor{};
            descriptor.vs = vs.Get();
            descriptor.ps = fs.Get();
            return ShaderPtr(device->CreateShader(descriptor));
        }
        DeviceFrameLease Begin()
        {
            auto result = context->TryBeginSlot();
            if (auto *lease = std::get_if<DeviceFrameLease>(&result))
                return *lease;
            ADD_FAILURE() << "No slot";
            return {};
        }
        std::shared_ptr<const FrozenFrameGraph> ClearGraph()
        {
            FrameGraphBuilder builder;
            auto              image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
            FrameRasterPass   pass;
            pass.color = image;
            pass.draws.push_back({ShaderOf(), {}, {3, 1, 0, 0}, {}});
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(image)));
            return std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
        }
        void AwaitCompletions(size_t count)
        {
            const auto deadline  = std::chrono::steady_clock::now() + 5s;
            size_t     completed = 0;
            while (completed < count && std::chrono::steady_clock::now() < deadline) {
                std::array<FrameCompletion, 2> output;
                const auto                     received = context->PollCompleted(output);
                for (size_t i = 0; i < received; ++i)
                    EXPECT_EQ(output[i].status, FrameCompletionStatus::COMPLETE);
                completed += received;
                if (!received)
                    std::this_thread::sleep_for(1ms);
            }
            EXPECT_EQ(completed, count);
        }
        BufferPtr Readback(FrameGraphBuilder &builder, GraphImageHandle image)
        {
            auto buffer = BufferOf(BufferUsageFlagBit::TRANSFER_DST);
            auto target = std::get<GraphBufferHandle>(builder.Import(buffer, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
            BufferImageCopy region{};
            region.subRange.layers = 1;
            region.imageExtent     = {8, 8, 1};
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(
                builder.Copy(BufferImageCopyPass{target, image, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(GraphBufferExport{target, 0, 256})));
            return buffer;
        }
        std::pair<std::shared_ptr<const FrozenFrameGraph>, BufferPtr> FragmentGraph()
        {
            FrameGraphBuilder builder;
            const auto        image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
            const auto        red = ShaderOf(), green = ShaderOf(true);
            FrameRasterPass   raster;
            raster.color = image;
            // Three fragments overwrite one another; only the last (green) may determine the left half.
            for (uint32_t i = 0; i < 65; ++i)
                raster.draws.push_back({i == 64 ? green : red, {}, {3, 1, 0, i}, {}});
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(raster))));
            for (uint32_t i = 65; i < 70; ++i) {
                FrameRasterPass next;
                next.color = image;
                next.load  = LoadOp::LOAD;
                next.draws.push_back({i == 69 ? red : green, {}, {3, 1, 0, i}, {{4, 0}, {4, 8}}});
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(next))));
            }
            auto buffer = Readback(builder, image);
            return {std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()), std::move(buffer)};
        }
        void Execute(const std::shared_ptr<const FrozenFrameGraph> &graph, const std::function<void(const VulkanCompiledFrame &)> &inspect = {})
        {
            auto compiled = context->Compile(Begin(), graph);
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
            auto        plan   = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
            const auto *native = static_cast<const VulkanCompiledFrame *>(plan.get());
            ASSERT_NE(native, nullptr);
            if (inspect)
                inspect(*native);
            auto dispatch = context->Dispatch(plan);
            ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatch));
            auto collected = AwaitEncoding(*context, std::get<EncodingTicket>(dispatch));
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
            auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
            AwaitCompletions(1);
        }
        VulkanDevice                       *device = nullptr;
        std::unique_ptr<DeviceFrameContext> context;
    };
} // namespace

TEST_P(VulkanFrameCompilerFixture, FullscreenGeneratedTriangleReadsBackThroughOwnCompiler)
{
    FrameGraphBuilder builder;
    auto              image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    FrameRasterPass   pass;
    pass.color = image;
    pass.draws.push_back({ShaderOf(), {}, {3, 1, 0, 0}, {}});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Fullscreen(std::move(pass))));
    auto readback = Readback(builder, image);
    auto graph    = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    Execute(graph);
    auto *pixels = readback->Map();
    ASSERT_TRUE(pixels);
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_EQ(pixels[i * 4], 255);
        EXPECT_EQ(pixels[i * 4 + 1], 0);
        EXPECT_EQ(pixels[i * 4 + 2], 0);
        EXPECT_EQ(pixels[i * 4 + 3], 255);
    }
    readback->UnMap();
}

TEST_P(VulkanFrameCompilerFixture, ComputeUniformOffsetsAreIndependentOfDeclarationOrder)
{
    const auto function = CompileGpuFunction(*device, kGpuComputeUniformSource, ShaderStageFlagBit::CS, "CSMain");
    ASSERT_TRUE(function);
    for (const bool dynamic : {false, true}) {
        Shader::Descriptor descriptor{};
        descriptor.cs              = function.Get();
        descriptor.bindingPolicies = ComputeUniformPolicies(dynamic);
        auto shader                = ShaderPtr(device->CreateShader(descriptor));
        ASSERT_TRUE(shader);
        auto [graph, readback] = ComputeUniformGraph(*device, std::move(shader), dynamic);
        ASSERT_TRUE(graph);
        Execute(graph);
        const auto *values = reinterpret_cast<const uint32_t *>(readback->Map());
        ASSERT_TRUE(values);
        for (uint32_t i = 0; i < 4; ++i)
            EXPECT_EQ(values[i], 81u + i);
        readback->UnMap();
    }
}

TEST_P(VulkanFrameCompilerFixture, ComputeSamplersRemainPinnedAfterCpuGraphRelease)
{
    const auto function = CompileGpuFunction(*device, kGpuComputeSampleSource, ShaderStageFlagBit::CS, "CSMain");
    ASSERT_TRUE(function);
    Shader::Descriptor descriptor{};
    descriptor.cs = function.Get();
    auto shader   = ShaderPtr(device->CreateShader(descriptor));
    ASSERT_TRUE(shader);
    auto sampler = SamplerPtr(device->CreateSampler({}));
    ASSERT_TRUE(sampler);
    FrameGraphBuilder builder;
    const auto        image = std::get<GraphImageHandle>(builder.Import(StorageImageOf()));
    const auto storage      = std::get<GraphBufferHandle>(builder.Import(StorageBufferOf(), CopyBufferState::COMMON, CopyBufferState::COMMON, false));
    FrameComputePass fill;
    fill.shader  = ComputeImageShaderOf();
    fill.groupsX = fill.groupsY = 8;
    fill.images.push_back({0, 0, image, {}, FrameAccessMode::WRITE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(fill))));
    FrameComputePass sample;
    sample.shader = shader;
    sample.images.push_back({0, 0, image, {}, FrameAccessMode::READ});
    sample.samplers.push_back({0, 1, sampler});
    sample.buffers.push_back({1, 0, {storage, 0, 4}, 0, FrameAccessMode::WRITE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(sample))));
    auto       readback = BufferOf(BufferUsageFlagBit::TRANSFER_DST);
    const auto target   = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({storage, target, 0, 0, 4})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({target, 0, 4})));
    auto                                  graph    = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    std::weak_ptr<const FrozenFrameGraph> weak     = graph;
    auto                                  compiled = context->Compile(Begin(), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    graph.reset();
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = AwaitEncoding(*context, std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    EXPECT_TRUE(weak.expired());
    EXPECT_GE(sampler->GetRef(), 2u);
    EXPECT_GE(shader->GetRef(), 2u);
    AwaitCompletions(1);
    EXPECT_EQ(sampler->GetRef(), 1u);
    EXPECT_EQ(shader->GetRef(), 1u);
    const auto *values = reinterpret_cast<const uint32_t *>(readback->Map());
    ASSERT_TRUE(values);
    EXPECT_EQ(values[0], 255u);
    readback->UnMap();
}

TEST_P(VulkanFrameCompilerFixture, ComputeStorageBufferWritesReadBack)
{
    FrameGraphBuilder builder;
    auto              storage = StorageBufferOf();
    const auto        source =
        std::get<GraphBufferHandle>(builder.Import(FrameGraphBuffer{storage, 64, CopyBufferState::COMMON, CopyBufferState::COMMON, false}));
    auto             readback = BufferOf(BufferUsageFlagBit::TRANSFER_DST);
    const auto       target   = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
    FrameComputePass compute;
    compute.shader  = ComputeShaderOf();
    compute.groupsX = 4;
    compute.buffers.push_back({0, 0, {source, 0, 64}, 0, FrameAccessMode::WRITE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(compute))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(BufferCopyPass{source, target, 0, 0, 64})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(GraphBufferExport{target, 0, 64})));
    auto graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    Execute(graph);
    auto *values = static_cast<const uint32_t *>(static_cast<const void *>(readback->Map()));
    ASSERT_TRUE(values);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], i + 7);
    readback->UnMap();
}

TEST_P(VulkanFrameCompilerFixture, ComputeStorageImageIsSampledByRaster)
{
    FrameGraphBuilder builder;
    const auto        source = std::get<GraphImageHandle>(builder.Import(StorageImageOf()));
    const auto        output = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    FrameComputePass  compute;
    compute.shader  = ComputeImageShaderOf();
    compute.groupsX = 8;
    compute.groupsY = 8;
    compute.images.push_back({0, 0, source, {}, FrameAccessMode::WRITE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(compute))));
    FrameRasterPass raster;
    raster.color = output;
    RasterDraw draw;
    draw.shader  = SampleShaderOf();
    draw.command = {3, 1, 0, 0};
    draw.images.push_back({0, 0, source, {}});
    draw.samplers.push_back({0, 1, SamplerPtr(device->CreateSampler({}))});
    raster.draws.push_back(std::move(draw));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(raster))));
    auto readback = Readback(builder, output);
    auto graph    = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    Execute(graph);
    auto *pixels = readback->Map();
    ASSERT_TRUE(pixels);
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_EQ(pixels[i * 4], 0);
        EXPECT_EQ(pixels[i * 4 + 1], 255);
        EXPECT_EQ(pixels[i * 4 + 2], 0);
        EXPECT_EQ(pixels[i * 4 + 3], 255);
    }
    readback->UnMap();
}

TEST_P(VulkanFrameCompilerFixture, CustomReadsStorageImageAndBuffer)
{
    const auto function = CompileGpuFunction(*device, kGpuStorageReadSource, ShaderStageFlagBit::CS, "CSMain");
    ASSERT_TRUE(function);
    Shader::Descriptor desc{};
    desc.cs           = function.Get();
    const auto shader = ShaderPtr(device->CreateShader(desc));
    ASSERT_TRUE(shader);
    auto [graph, readback] = CustomStorageReadGraph(*device, ComputeImageShaderOf(), ComputeShaderOf(), shader);
    ASSERT_TRUE(graph);
    NativeProbe probe(*device);
    probe.inspectStorageStages = true;
    Execute(graph);
    EXPECT_GT(probe.storageSources, 0u);
    EXPECT_GT(probe.storageDestinations, 0u);
    const auto *values = reinterpret_cast<const uint32_t *>(readback->Map());
    ASSERT_TRUE(values);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], 262u + i);
    readback->UnMap();
}

TEST_P(VulkanFrameCompilerFixture, CustomComputeWritesDeclaredBufferReadBack)
{
    auto                        shader = ComputeShaderOf();
    ComputePipeline::Descriptor pipelineDesc{};
    pipelineDesc.cs = shader.Get();
    CounterPtr<ComputePipeline> pipeline(device->CreatePipelineState(pipelineDesc));
    ASSERT_TRUE(pipeline);
    ResourceGroup::Descriptor groupDesc{shader.Get(), 0};
    CounterPtr<ResourceGroup> group(device->CreateResourceGroup(groupDesc));
    ASSERT_TRUE(group);
    auto descriptor = group->CreateEncoder();
    ASSERT_TRUE(descriptor);

    FrameGraphBuilder builder;
    auto              storage = StorageBufferOf();
    const auto        declared =
        std::get<GraphBufferHandle>(builder.Import(FrameGraphBuffer{storage, 64, CopyBufferState::COMMON, CopyBufferState::COMMON, false}));
    ASSERT_TRUE(descriptor->WriteBuffer(0, storage.Get(), 0, 64, 0));
    ASSERT_TRUE(descriptor->End());
    auto callback            = std::make_shared<CustomComputeEncoder>();
    callback->pipeline       = pipeline.Get();
    callback->group          = group.Get();
    auto            readback = BufferOf(BufferUsageFlagBit::TRANSFER_DST);
    const auto      target   = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
    FrameCustomPass custom{"custom compute"};
    custom.encoder = callback;
    custom.buffers.push_back({0, 0, {declared, 0, 64}, 0, FrameAccessMode::WRITE, FrameBufferBindingKind::STORAGE});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Custom(std::move(custom))));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(BufferCopyPass{declared, target, 0, 0, 64})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(GraphBufferExport{target, 0, 64})));
    auto graph = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    Execute(graph);
    auto *values = static_cast<const uint32_t *>(static_cast<const void *>(readback->Map()));
    ASSERT_TRUE(values);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(values[i], i + 7);
    readback->UnMap();
}

TEST_P(VulkanFrameCompilerFixture, InvalidResourceOwnersAndImageDescriptorsDoNotConsumeLease)
{
    const auto [graph, readback] = FragmentGraph();
    const auto lease             = Begin();
    for (uint32_t invalid = 0; invalid < 4; ++invalid) {
        auto data = graph->GetData();
        switch (invalid) {
        case 0: data.buffers[0].buffer = BufferPtr(new Buffer()); break;
        case 1: data.images[0].image = ImagePtr(new Image()); break;
        case 2: std::get<FrameRasterPass>(data.passes[0]).draws[0].shader = ShaderPtr(new Shader()); break;
        case 3: data.images[0].descriptor.usage |= ImageUsageFlagBit::SAMPLED; break;
        }
        auto compiled = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
        EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::INVALID_ARGUMENT);
    }
    auto valid = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(valid));
    std::get<std::unique_ptr<BackendCompiledFrame>>(valid).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(VulkanFrameCompilerFixture, BothEncodersPreserveIntermediateContentsAndRetainNativeObjects)
{
    NativeProbe probe(*device);
    for (uint32_t round = 0; round < 3; ++round) {
        std::array<std::array<BufferPtr, 2>, 2>  readbacks;
        std::array<std::weak_ptr<const void>, 2> weak;
        for (uint32_t slot = 0; slot < 2; ++slot) {
            FrameGraphBuilder builder;
            auto              image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
            BufferImageCopy   region{};
            region.subRange.layers = 1;
            region.imageExtent     = {8, 8, 1};
            if (slot == 0) {
                auto upload = BufferOf(BufferUsageFlagBit::TRANSFER_SRC);
                ASSERT_TRUE(upload);
                auto *pixels = static_cast<uint8_t *>(upload->Map());
                ASSERT_TRUE(pixels);
                for (size_t i = 0; i < 64; ++i) {
                    pixels[i * 4] = pixels[i * 4 + 1] = 0;
                    pixels[i * 4 + 2] = pixels[i * 4 + 3] = 255;
                }
                upload->UnMap();
                auto src = std::get<GraphBufferHandle>(builder.Import(upload, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true));
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy(BufferImageCopyPass{src, image, region})));
            }
            for (uint32_t passIndex = 0; passIndex < 2; ++passIndex) {
                FrameRasterPass pass;
                pass.color = image;
                pass.load  = slot == 1 && passIndex == 0 ? LoadOp::CLEAR : LoadOp::LOAD;
                pass.clear = ClearValue{0.f, 0.f, 1.f, 1.f};
                RasterDraw draw{ShaderOf(passIndex == 1), {}, {3, 1, 0, 0}, {{int32_t(passIndex * 4), 0}, {4, 8}}};
                ASSERT_TRUE(draw.shader);
                pass.draws.push_back(std::move(draw));
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
                auto &readback = readbacks[slot][passIndex];
                readback       = BufferOf(BufferUsageFlagBit::TRANSFER_DST);
                ASSERT_TRUE(readback);
                auto dst = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
                    builder.Copy(BufferImageCopyPass{dst, image, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(GraphBufferExport{dst, 0, 256})));
            }
            FrameRasterPass unused;
            unused.color = image;
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(unused))));
            auto graph    = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
            weak[slot]    = TrackGraphGpuPayload(graph);
            auto analysis = AnalyzeFrameGraph(*graph);
            ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analysis));
            EXPECT_EQ(std::get<FrameGraphAnalysis>(analysis).livePasses.size(), slot == 0 ? 5u : 4u);
            const auto lease  = Begin();
            auto       result = context->Compile(lease, graph);
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(result));
            auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(result));
            EvictFrameCaches(*device);
            graph.reset();
            auto dispatched = context->Dispatch(plan);
            ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
            EXPECT_FALSE(plan);
            auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
            auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
            EXPECT_FALSE(weak[slot].expired());
            EXPECT_EQ(probe.pipelinesDestroyed, round * 4);
        }
        AwaitCompletions(2);
        EXPECT_EQ(probe.pipelinesDestroyed, (round + 1) * 4);
        for (uint32_t slot = 0; slot < 2; ++slot) {
            EXPECT_TRUE(weak[slot].expired());
            for (uint32_t passIndex = 0; passIndex < 2; ++passIndex) {
                auto *pixels = static_cast<const uint8_t *>(readbacks[slot][passIndex]->Map());
                ASSERT_TRUE(pixels);
                for (uint32_t y = 0; y < 8; ++y)
                    for (uint32_t x = 0; x < 8; ++x) {
                        const size_t index = (y * 8 + x) * 4;
                        EXPECT_EQ(pixels[index], x < 4 ? 255 : 0);
                        EXPECT_EQ(pixels[index + 1], x >= 4 && passIndex == 1 ? 255 : 0);
                        EXPECT_EQ(pixels[index + 2], x >= 4 && passIndex == 0 ? 255 : 0);
                        EXPECT_EQ(pixels[index + 3], 255);
                    }
                readbacks[slot][passIndex]->UnMap();
            }
        }
    }
    const bool renderPass = GetParam() == VulkanFrameGraphMode::RENDER_PASS;
    EXPECT_EQ(probe.renderPassBegins, renderPass ? 12u : 0u);
    EXPECT_EQ(probe.dynamicBegins, renderPass ? 0u : 12u);
    EXPECT_EQ(probe.renderPassPipelines, renderPass ? 12u : 0u);
    EXPECT_EQ(probe.dynamicPipelines, renderPass ? 0u : 12u);
    EXPECT_EQ(probe.renderPassesDestroyed, renderPass ? 12u : 0u);
    EXPECT_EQ(probe.framebuffersDestroyed, renderPass ? 12u : 0u);
}

TEST_P(VulkanFrameCompilerFixture, ParallelFragmentsOverlapAndMatchSerialAcrossTwoSlots)
{
    auto [serialGraph, serialBuffer] = FragmentGraph();
    Execute(serialGraph);
    std::array<uint8_t, 256> reference{};
    std::memcpy(reference.data(), serialBuffer->Map(), reference.size());
    serialBuffer->UnMap();
    context.reset(device->CreateFrameContext({2, 2, 93, 16, GetParam()}));
    ASSERT_TRUE(context);
    NativeProbe    native(*device);
    RecordingProbe probe(*device);
    Finally        cleanup{[&] {
        probe.Release();
        context.reset();
    }};
    probe.gateBegin    = true;
    probe.reverseFirst = true;
    std::array<BufferPtr, 2>    outputs;
    const std::vector<uint32_t> order{0, 32, 64, 65, 66, 67, 68, 69};
    for (uint32_t frame = 0; frame < 2; ++frame) {
        auto [graph, buffer] = FragmentGraph();
        outputs[frame]       = std::move(buffer);
        auto compiled        = context->Compile(Begin(), graph);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto        plan  = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        const auto *typed = static_cast<const VulkanCompiledFrame *>(plan.get());
        ASSERT_EQ(typed->encodeTasks.size(), order.size());
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        const auto ticket = std::get<EncodingTicket>(dispatched);
        auto       wrong  = ticket;
        ++wrong.sessionId;
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(wrong)));
        auto collected = AwaitEncoding(*context, ticket);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(ticket)));
        ASSERT_EQ(probe.executedOrder.size(), (frame + 1) * order.size());
        EXPECT_TRUE(std::equal(order.begin(), order.end(), probe.executedOrder.begin() + frame * order.size()));
        std::set<VkCommandBuffer> commands(probe.executedCommands.begin() + frame * order.size(), probe.executedCommands.end());
        EXPECT_EQ(commands.size(), order.size());
        {
            std::lock_guard lock(probe.mutex);
            for (auto command : commands)
                probe.protectedPools.insert(probe.commands.at(command).pool);
        }
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    }
    EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
    EXPECT_GE(probe.maxConcurrent, 2u);
    EXPECT_EQ(probe.threads.size(), 2u);
    EXPECT_NE(probe.completedOrder.front(), 0u);
    EXPECT_EQ(probe.primaryBegins, 2u);
    AwaitCompletions(2);
    for (auto &buffer : outputs) {
        EXPECT_EQ(std::memcmp(reference.data(), buffer->Map(), reference.size()), 0);
        buffer->UnMap();
    }
    for (uint32_t y = 0; y < 8; ++y)
        for (uint32_t x = 0; x < 8; ++x) {
            EXPECT_EQ(reference[(y * 8 + x) * 4], x < 4 ? 0 : 255);
            EXPECT_EQ(reference[(y * 8 + x) * 4 + 1], x < 4 ? 255 : 0);
        }
    probe.protectedPools.clear();
    Execute(serialGraph);
    EXPECT_EQ(probe.primaryBegins, 3u);
}

TEST_P(VulkanFrameCompilerFixture, ParallelFailureAndCancellationDrainBeforeReleasingLease)
{
    for (bool fail : {false, true}) {
        context.reset(device->CreateFrameContext({2, 2, 94, 16, GetParam()}));
        ASSERT_TRUE(context);
        auto [graph, buffer]                       = FragmentGraph();
        std::weak_ptr<const FrozenFrameGraph> weak = graph;
        NativeProbe                           native(*device);
        RecordingProbe                        probe(*device);
        Finally                               cleanup{[&] {
            probe.Release();
            context.reset();
        }};
        probe.gateBegin     = true;
        probe.holdEnd       = true;
        probe.failFirstEnd  = fail;
        const auto lease    = Begin();
        auto       compiled = context->Compile(lease, graph);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        EvictFrameCaches(*device);
        graph.reset();
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        const auto ticket = std::get<EncodingTicket>(dispatched);
        {
            std::unique_lock lock(probe.mutex);
            ASSERT_TRUE(probe.changed.wait_for(lock, 5s, [&] { return probe.arrivals >= 2 && probe.finished >= 1; }));
        }
        EXPECT_TRUE(std::holds_alternative<FramePending>(context->TryCollect(ticket)));
        EXPECT_FALSE(weak.expired());
        EXPECT_EQ(native.pipelinesDestroyed, 0u);
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
        EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
        EXPECT_TRUE(std::holds_alternative<FramePending>(context->TryCollect(ticket)));
        EXPECT_FALSE(weak.expired());
        probe.Release();
        auto collected = AwaitEncoding(*context, ticket);
        if (fail) {
            ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
            EXPECT_EQ(std::get<EncodingFailed>(collected).error.cause.nativeCode, VK_ERROR_OUT_OF_HOST_MEMORY);
        } else
            EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(collected));
        EXPECT_TRUE(weak.expired());
        EXPECT_EQ(native.pipelinesDestroyed, GetParam() == VulkanFrameGraphMode::RENDER_PASS ? 7u : 2u);
        EXPECT_EQ(probe.primaryBegins, 0u);
        EXPECT_TRUE(probe.executedCommands.empty());
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        probe.holdEnd      = false;
        probe.failFirstEnd = false;
        Execute(ClearGraph());
    }
}

TEST_P(VulkanFrameCompilerFixture, CustomFailureStopsEncodingAndPreservesDeviceLoss)
{
    class Callback final : public FrameCustomPassEncoder {
    public:
        mutable uint32_t     calls = 0;
        bool                 fail  = true;
        FrameOperationResult Encode(FrameCustomPassContext &) const noexcept override
        {
            ++calls;
            if (fail)
                return FrameError{FrameErrorCode::ENCODE_FAILED, "Custom callback failed"};
            return FrameOperationComplete{};
        }
    };
    for (bool deviceLost : {false, true}) {
        context.reset(device->CreateFrameContext({1, 1, 101, 16, GetParam()}));
        ASSERT_TRUE(context);
        RecordingProbe probe(*device);
        Finally        cleanup{[&] { context.reset(); }};
        if (deviceLost) {
            probe.fault       = RecordingProbe::Fault::PRIMARY_END;
            probe.faultResult = VK_ERROR_DEVICE_LOST;
        }
        auto first   = std::make_shared<Callback>();
        auto second  = std::make_shared<Callback>();
        second->fail = false;
        FrameGraphBuilder builder;
        for (const auto &callback : {first, second}) {
            FrameCustomPass pass;
            pass.encoder    = callback;
            pass.sideEffect = true;
            builder.Custom(std::move(pass));
        }
        const auto lease    = Begin();
        auto       compiled = context->Compile(lease, std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        const auto collected = AwaitEncoding(*context, std::get<EncodingTicket>(dispatched));
        ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
        const auto &error = std::get<EncodingFailed>(collected).error;
        if (deviceLost)
            EXPECT_EQ(error.cause.code, GpuErrorCode::DEVICE_LOST);
        else
            EXPECT_EQ(error.message, "Custom callback failed");
        EXPECT_EQ(first->calls, 1u);
        EXPECT_EQ(second->calls, 0u);
        EXPECT_EQ(probe.submitCalls, 0u);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    }
}

TEST_P(VulkanFrameCompilerFixture, ResourceFreeCustomExecutes)
{
    class Callback final : public FrameCustomPassEncoder {
    public:
        mutable uint32_t     calls = 0;
        FrameOperationResult Encode(FrameCustomPassContext &) const noexcept override
        {
            ++calls;
            return FrameOperationComplete{};
        }
    };
    const auto        callback = std::make_shared<Callback>();
    FrameGraphBuilder builder;
    FrameCustomPass   pass;
    pass.encoder    = callback;
    pass.sideEffect = true;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Custom(std::move(pass))));
    Execute(std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()));
    EXPECT_EQ(callback->calls, 1u);
}

TEST_P(VulkanFrameCompilerFixture, DeviceLostInLaterFragmentCannotBeHiddenByEarlierFailure)
{
    context.reset(device->CreateFrameContext({1, 2, 101, 16, GetParam()}));
    ASSERT_TRUE(context);
    NativeProbe    native(*device);
    RecordingProbe probe(*device);
    Finally        cleanup{[&] {
        probe.Release();
        context.reset();
    }};
    probe.gateBegin                                = true;
    probe.holdEnd                                  = true;
    probe.endFailures                              = {{0, VK_ERROR_OUT_OF_HOST_MEMORY}, {32, VK_ERROR_DEVICE_LOST}};
    auto [graph, buffer]                           = FragmentGraph();
    std::weak_ptr<const FrozenFrameGraph> weak     = graph;
    const auto                            lease    = Begin();
    auto                                  compiled = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    graph.reset();
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    const auto ticket = std::get<EncodingTicket>(dispatched);
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.changed.wait_for(lock, 5s, [&] { return probe.arrivals >= 2 && probe.finished >= 1; }));
    }
    EXPECT_TRUE(std::holds_alternative<FramePending>(context->TryCollect(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    EXPECT_FALSE(weak.expired());
    probe.Release();
    auto collected = AwaitEncoding(*context, ticket);
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
    EXPECT_EQ(std::get<EncodingFailed>(collected).error.cause.code, GpuErrorCode::DEVICE_LOST);
    EXPECT_EQ(std::get<EncodingFailed>(collected).error.cause.nativeCode, VK_ERROR_DEVICE_LOST);
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(probe.primaryBegins, 0u);
    EXPECT_EQ(probe.submitCalls, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
}

TEST_P(VulkanFrameCompilerFixture, NativeEncodingFaultsDrainWithoutSubmissionAndAllowRetry)
{
    using Fault = RecordingProbe::Fault;
    for (auto fault : {Fault::WORKER_RESET, Fault::WORKER_ALLOCATE, Fault::WORKER_BEGIN, Fault::NONE, Fault::PRIMARY_BEGIN, Fault::PRIMARY_END}) {
        SCOPED_TRACE(static_cast<int>(fault));
        context.reset(device->CreateFrameContext({1, 2, 102, 16, GetParam()}));
        ASSERT_TRUE(context);
        NativeProbe    native(*device);
        RecordingProbe probe(*device);
        Finally        cleanup{[&] { context.reset(); }};
        probe.fault = fault;
        if (fault == Fault::NONE)
            probe.endFailures = {{32, VK_ERROR_OUT_OF_DEVICE_MEMORY}};
        auto [graph, buffer]                           = FragmentGraph();
        std::weak_ptr<const FrozenFrameGraph> weak     = graph;
        const auto                            lease    = Begin();
        auto                                  compiled = context->Compile(lease, graph);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        EvictFrameCaches(*device);
        graph.reset();
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        const auto ticket = std::get<EncodingTicket>(dispatched);
        EXPECT_FALSE(plan);
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Dispatch(plan)));
        auto collected = AwaitEncoding(*context, ticket);
        ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
        const auto &error = std::get<EncodingFailed>(collected).error;
        EXPECT_EQ(error.code, FrameErrorCode::ENCODE_FAILED);
        EXPECT_EQ(error.cause.code, GpuErrorCode::OUT_OF_MEMORY);
        EXPECT_EQ(error.cause.nativeCode, fault == Fault::NONE ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_ERROR_OUT_OF_HOST_MEMORY);
        EXPECT_TRUE(weak.expired());
        EXPECT_EQ(native.pipelinesDestroyed, GetParam() == VulkanFrameGraphMode::RENDER_PASS ? 7u : 2u);
        EXPECT_EQ(probe.concurrent, 0u);
        EXPECT_EQ(probe.submitCalls, 0u);
        EXPECT_EQ(probe.fenceResets, 0u);
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(ticket)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Cancel(ticket)));
        std::array<FrameCompletion, 1> completions;
        EXPECT_EQ(context->PollCompleted(completions), 0u);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        probe.fault = Fault::NONE;
        probe.endFailures.clear();
        Execute(ClearGraph());
        EXPECT_EQ(probe.submitCalls, 1u);
    }
}

TEST_P(VulkanFrameCompilerFixture, WrongTicketsAndLateCancellationCannotConsumeAnotherSession)
{
    context.reset(device->CreateFrameContext({1, 2, 103, 16, GetParam()}));
    ASSERT_TRUE(context);
    RecordingProbe probe(*device);
    Finally        cleanup{[&] {
        probe.Release();
        context.reset();
    }};
    probe.gateBegin                                = true;
    probe.holdEnd                                  = true;
    auto [graph, buffer]                           = FragmentGraph();
    std::weak_ptr<const FrozenFrameGraph> weak     = graph;
    const auto                            lease    = Begin();
    auto                                  compiled = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    graph.reset();
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    const auto ticket = std::get<EncodingTicket>(dispatched);
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.changed.wait_for(lock, 5s, [&] { return probe.arrivals >= 2; }));
    }
    for (int field = 0; field < 8; ++field) {
        auto wrong = ticket;
        if (field == 0)
            wrong.frame.lease.context.backend = API::DX12;
        if (field == 1)
            ++wrong.frame.lease.context.deviceEpoch;
        if (field == 2)
            ++wrong.frame.lease.context.contextId;
        if (field == 3)
            wrong.frame.lease.slot = UINT32_MAX;
        if (field == 4)
            ++wrong.frame.lease.generation;
        if (field == 5)
            ++wrong.frame.graphId;
        if (field == 6)
            ++wrong.frame.planId;
        if (field == 7)
            ++wrong.sessionId;
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(wrong)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->Cancel(wrong)));
    }
    EXPECT_TRUE(std::holds_alternative<FramePending>(context->TryCollect(ticket)));
    probe.Release();
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.changed.wait_for(lock, 5s, [&] { return probe.finished == 8; }));
    }
    EXPECT_FALSE(weak.expired());
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(AwaitEncoding(*context, ticket)));
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(probe.primaryBegins, 0u);
    EXPECT_EQ(probe.submitCalls, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Cancel(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));

    const auto next = Begin();
    EXPECT_NE(next.generation, lease.generation);
    compiled = context->Compile(next, ClearGraph());
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryCollect(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Cancel(ticket)));
    auto collected = AwaitEncoding(*context, std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    EXPECT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(context->Submit(recorded)));
    EXPECT_EQ(probe.submitCalls, 1u);
    AwaitCompletions(1);
}

TEST_P(VulkanFrameCompilerFixture, ParallelRecordingSurvivesRejectedSubmitAndRetiresOnlyAfterCompletion)
{
    context.reset(device->CreateFrameContext({1, 2, 104, 16, GetParam()}));
    ASSERT_TRUE(context);
    NativeProbe    native(*device);
    RecordingProbe probe(*device);
    Finally        cleanup{[&] { context.reset(); }};
    auto [graph, buffer]               = FragmentGraph();
    std::weak_ptr<const void> weak     = TrackGraphGpuPayload(graph);
    const auto                lease    = Begin();
    auto                      compiled = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    EvictFrameCaches(*device);
    graph.reset();
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    const auto ticket    = std::get<EncodingTicket>(dispatched);
    auto       collected = AwaitEncoding(*context, ticket);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto       recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    const auto identity = recorded->GetIdentity();
    for (const auto &[command, info] : probe.commands)
        probe.protectedPools.insert(info.pool);
    probe.fenceResetResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    auto submitted         = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionRejected>(submitted));
    EXPECT_EQ(std::get<SubmissionRejected>(submitted).error.cause.nativeCode, VK_ERROR_OUT_OF_HOST_MEMORY);
    ASSERT_TRUE(recorded);
    EXPECT_EQ(recorded->GetIdentity(), identity);
    EXPECT_EQ(probe.submitCalls, 0u);
    probe.fenceResetResult = VK_SUCCESS;
    probe.submitResult     = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    submitted              = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionRejected>(submitted));
    EXPECT_EQ(std::get<SubmissionRejected>(submitted).error.cause.nativeCode, VK_ERROR_OUT_OF_DEVICE_MEMORY);
    ASSERT_TRUE(recorded);
    EXPECT_EQ(recorded->GetIdentity(), identity);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Cancel(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
    std::array<FrameCompletion, 1> completions;
    EXPECT_EQ(context->PollCompleted(completions), 0u);
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(native.pipelinesDestroyed, 0u);
    probe.submitResult = VK_SUCCESS;
    submitted          = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
    EXPECT_EQ(std::get<SubmissionAccepted>(submitted).token.serial, 1u);
    EXPECT_FALSE(recorded);
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(context->Submit(recorded)));
    probe.queryResult = VK_NOT_READY;
    EXPECT_EQ(context->PollCompleted(completions), 0u);
    EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(probe.primaryBegins, 1u);
    EXPECT_EQ(probe.arrivals, 8u);
    EXPECT_EQ(probe.submitCalls, 2u);
    probe.queryResult = VK_SUCCESS;
    AwaitCompletions(1);
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(native.pipelinesDestroyed, GetParam() == VulkanFrameGraphMode::RENDER_PASS ? 7u : 2u);
    probe.protectedPools.clear();
    Execute(ClearGraph());
}

TEST_P(VulkanFrameCompilerFixture, ParallelUncertainSubmissionAndQueryFailureRetainUntilTeardown)
{
    for (bool uncertain : {false, true}) {
        context.reset(device->CreateFrameContext({1, 2, 105, 16, GetParam()}));
        ASSERT_TRUE(context);
        NativeProbe    native(*device);
        RecordingProbe probe(*device);
        Finally        cleanup{[&] { context.reset(); }};
        auto [graph, buffer]               = FragmentGraph();
        std::weak_ptr<const void> weak     = TrackGraphGpuPayload(graph);
        const auto                lease    = Begin();
        auto                      compiled = context->Compile(lease, graph);
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
        auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        EvictFrameCaches(*device);
        graph.reset();
        auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        auto collected = AwaitEncoding(*context, std::get<EncodingTicket>(dispatched));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded        = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        probe.submitResult   = uncertain ? VK_ERROR_UNKNOWN : VK_SUCCESS;
        const auto submitted = context->Submit(recorded);
        EXPECT_EQ(std::holds_alternative<SubmissionIndeterminate>(submitted), uncertain);
        EXPECT_EQ(std::holds_alternative<SubmissionAccepted>(submitted), !uncertain);
        EXPECT_FALSE(recorded);
        std::array<FrameCompletion, 1> completions;
        probe.queryResult = VK_NOT_READY;
        EXPECT_EQ(context->PollCompleted(completions), 0u);
        EXPECT_FALSE(weak.expired());
        probe.queryResult = VK_ERROR_DEVICE_LOST;
        ASSERT_EQ(context->PollCompleted(completions), 1u);
        EXPECT_EQ(completions[0].status, FrameCompletionStatus::DEVICE_LOST);
        EXPECT_EQ(context->PollCompleted(completions), 0u);
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
        EXPECT_FALSE(weak.expired());
        EXPECT_EQ(native.pipelinesDestroyed, 0u);
        probe.requireGpuWait = true;
        context.reset();
        EXPECT_TRUE(probe.idleComplete);
        EXPECT_EQ(probe.poolsDestroyed, 3u);
        EXPECT_TRUE(weak.expired());
        EXPECT_EQ(native.pipelinesDestroyed, GetParam() == VulkanFrameGraphMode::RENDER_PASS ? 7u : 2u);
    }
}

TEST_P(VulkanFrameCompilerFixture, TeardownDrainsWorkersBeforeDestroyingPools)
{
    context.reset(device->CreateFrameContext({1, 2, 106, 16, GetParam()}));
    ASSERT_TRUE(context);
    NativeProbe    native(*device);
    RecordingProbe probe(*device);
    probe.gateBegin      = true;
    probe.holdEnd        = true;
    probe.holdIdle       = true;
    probe.requireGpuWait = true;
    std::future<void> destruction;
    Finally           cleanup{[&] {
        probe.Release();
        {
            std::lock_guard lock(probe.mutex);
            probe.releaseIdle = true;
            probe.changed.notify_all();
        }
        if (destruction.valid())
            destruction.get();
        context.reset();
    }};
    auto [graph, buffer]                           = FragmentGraph();
    std::weak_ptr<const FrozenFrameGraph> weak     = graph;
    auto                                  compiled = context->Compile(Begin(), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    EvictFrameCaches(*device);
    graph.reset();
    EXPECT_TRUE(std::holds_alternative<EncodingTicket>(context->Dispatch(plan)));
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.changed.wait_for(lock, 5s, [&] { return probe.arrivals >= 2; }));
    }
    std::promise<void> entered;
    auto               started = entered.get_future();
    destruction                = std::async(std::launch::async, [owned = std::move(context), &entered]() mutable {
        entered.set_value();
        owned.reset();
    });
    started.wait();
    EXPECT_EQ(destruction.wait_for(20ms), std::future_status::timeout);
    {
        std::lock_guard lock(probe.mutex);
        EXPECT_GT(probe.concurrent, 0u);
        EXPECT_EQ(probe.poolsDestroyed, 0u);
        EXPECT_FALSE(probe.idleEntered);
    }
    EXPECT_FALSE(weak.expired());
    probe.Release();
    {
        std::unique_lock lock(probe.mutex);
        ASSERT_TRUE(probe.changed.wait_for(lock, 5s, [&] { return probe.idleEntered; }));
        EXPECT_EQ(probe.concurrent, 0u);
        EXPECT_EQ(probe.poolsDestroyed, 0u);
        probe.releaseIdle = true;
        probe.changed.notify_all();
    }
    ASSERT_EQ(destruction.wait_for(5s), std::future_status::ready);
    destruction.get();
    EXPECT_TRUE(weak.expired());
    EXPECT_TRUE(probe.idleComplete);
    EXPECT_EQ(probe.poolsDestroyed, 3u);
    EXPECT_EQ(native.pipelinesDestroyed, GetParam() == VulkanFrameGraphMode::RENDER_PASS ? 7u : 2u);
    EXPECT_EQ(probe.primaryBegins, 0u);
    EXPECT_EQ(probe.submitCalls, 0u);
}

TEST_P(VulkanFrameCompilerFixture, ParallelBudgetFailureDoesNotStartWorkersOrProducePartialFrame)
{
    context.reset(device->CreateFrameContext({2, 2, 95, 2, GetParam()}));
    ASSERT_TRUE(context);
    RecordingProbe probe(*device);
    Finally        cleanup{[&] { context.reset(); }};
    auto [graph, buffer] = FragmentGraph();
    const auto lease     = Begin();
    auto       compiled  = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = AwaitEncoding(*context, std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<EncodingFailed>(collected));
    EXPECT_EQ(std::get<EncodingFailed>(collected).error.code, FrameErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(probe.primaryBegins, 0u);
    EXPECT_EQ(probe.arrivals, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    Execute(ClearGraph());
}

TEST_P(VulkanFrameCompilerFixture, CompatiblePassesFuseAndMatchSplitReference)
{
    std::array<std::array<uint8_t, 256>, 2> pixels{};
    const bool                              renderPass = GetParam() == VulkanFrameGraphMode::RENDER_PASS;
    for (uint32_t split = 0; split < 2; ++split) {
        NativeProbe       probe(*device);
        FrameGraphBuilder builder;
        auto              image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
        for (uint32_t i = 0; i < 3; ++i) {
            FrameRasterPass pass;
            pass.color = image;
            pass.load  = i ? LoadOp::LOAD : LoadOp::CLEAR;
            RasterDraw draw{ShaderOf(i == 1), {}, {3, 1, 0, 0}, {}};
            if (i != 1)
                draw.scissor = {{i == 0 ? 0 : 4, 0}, {4, 8}};
            draw.blend.blendEn  = true;
            draw.blend.srcColor = BlendFactor::ONE;
            draw.blend.dstColor = BlendFactor::ONE;
            draw.blend.srcAlpha = BlendFactor::ONE;
            pass.draws.push_back(std::move(draw));
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
            if (split && i != 2)
                Readback(builder, image);
        }
        auto buffer = Readback(builder, image);
        auto graph  = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
        Execute(graph, [&](const VulkanCompiledFrame &plan) {
            ASSERT_EQ(plan.rasterPasses.size(), 3u);
            for (uint32_t i = 0; i < 3; ++i) {
                const auto &pass = plan.rasterPasses[i];
                EXPECT_EQ(pass.logicalPass, split ? i * 2 : i);
                EXPECT_EQ(pass.nativeScope, renderPass && !split ? 0 : split ? i * 2 : i);
                if (renderPass) {
                    ASSERT_TRUE(std::holds_alternative<VulkanRenderPassInheritance>(pass.inheritance));
                    auto       info  = std::get<VulkanRenderPassInheritance>(pass.inheritance).GetInfo();
                    const auto scope = split ? i : 0;
                    ASSERT_LT(scope, probe.framebuffers.size());
                    EXPECT_EQ(info.renderPass, probe.framebuffers[scope].first);
                    EXPECT_EQ(info.framebuffer, probe.framebuffers[scope].second);
                    EXPECT_EQ(info.subpass, split ? 0 : i);
                    EXPECT_EQ(info.subpass, probe.pipelineSubpasses[i]);
                    EXPECT_EQ(info.pNext, nullptr);
                } else {
                    ASSERT_TRUE(std::holds_alternative<VulkanRenderingInheritance>(pass.inheritance));
                    const auto &inheritance = std::get<VulkanRenderingInheritance>(pass.inheritance);
                    auto        info        = inheritance.GetInfo();
                    EXPECT_EQ(info.colorAttachmentCount, 1u);
                    EXPECT_EQ(*info.pColorAttachmentFormats, VK_FORMAT_R8G8B8A8_UNORM);
                    EXPECT_EQ(info.rasterizationSamples, VK_SAMPLE_COUNT_1_BIT);
                    EXPECT_EQ(info.viewMask, 0u);
                    EXPECT_EQ(info.depthAttachmentFormat, VK_FORMAT_UNDEFINED);
                    EXPECT_EQ(info.stencilAttachmentFormat, VK_FORMAT_UNDEFINED);
                }
            }
        });
        auto *mapped = buffer->Map();
        ASSERT_NE(mapped, nullptr);
        std::memcpy(pixels[split].data(), mapped, pixels[split].size());
        buffer->UnMap();
        EXPECT_EQ(probe.renderPassBegins, renderPass ? (split ? 3u : 1u) : 0u);
        EXPECT_EQ(probe.dynamicBegins, renderPass ? 0u : 3u);
        EXPECT_EQ(probe.subpassTransitions, renderPass && !split ? 2u : 0u);
        EvictFrameCaches(*device);
        EXPECT_EQ(probe.pipelinesDestroyed, 3u);
        EXPECT_EQ(probe.renderPassesDestroyed, renderPass ? (split ? 2u : 1u) : 0u);
        if (renderPass)
            EXPECT_EQ(probe.subpassCounts, split ? std::vector<uint32_t>({1, 1}) : std::vector<uint32_t>({3}));
    }
    EXPECT_EQ(pixels[0], pixels[1]);
    for (size_t i = 0; i < 64; ++i) {
        EXPECT_EQ(pixels[0][i * 4], 255);
        EXPECT_EQ(pixels[0][i * 4 + 1], 255);
        EXPECT_EQ(pixels[0][i * 4 + 2], 0);
        EXPECT_EQ(pixels[0][i * 4 + 3], 255);
    }
}

TEST_P(VulkanFrameCompilerFixture, ClearAndAttachmentChangesSplitNativeScopes)
{
    NativeProbe       probe(*device);
    FrameGraphBuilder builder;
    auto              a = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    auto              b = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    FrameRasterPass   first;
    first.color      = a;
    first.clear      = ClearValue{1.f, 0.f, 0.f, 1.f};
    first.sideEffect = true;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(first)));
    first.clear = ClearValue{0.f, 1.f, 0.f, 1.f};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(first)));
    first.color = b;
    first.clear = ClearValue{0.f, 0.f, 1.f, 1.f};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(first)));
    FrameRasterPass last;
    last.color = a;
    last.load  = LoadOp::LOAD;
    last.draws.push_back({ShaderOf(), {}, {3, 1, 0, 0}, {{0, 0}, {4, 8}}});
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(last))));
    auto resultA = Readback(builder, a);
    auto resultB = Readback(builder, b);
    auto graph   = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    Execute(graph, [&](const VulkanCompiledFrame &plan) {
        ASSERT_EQ(plan.rasterPasses.size(), 4u);
        for (uint32_t i = 0; i < 4; ++i) {
            EXPECT_EQ(plan.rasterPasses[i].logicalPass, i);
            EXPECT_EQ(plan.rasterPasses[i].nativeScope, i);
        }
    });
    const bool renderPass = GetParam() == VulkanFrameGraphMode::RENDER_PASS;
    EXPECT_EQ(probe.renderPassBegins, renderPass ? 4u : 0u);
    EXPECT_EQ(probe.dynamicBegins, renderPass ? 0u : 4u);
    EXPECT_EQ(probe.subpassTransitions, 0u);
    for (uint32_t imageIndex = 0; imageIndex < 2; ++imageIndex) {
        auto  buffer = imageIndex ? resultB : resultA;
        auto *pixels = static_cast<uint8_t *>(buffer->Map());
        ASSERT_NE(pixels, nullptr);
        for (uint32_t i = 0; i < 64; ++i) {
            EXPECT_EQ(pixels[i * 4], imageIndex == 0 && i % 8 < 4 ? 255 : 0);
            EXPECT_EQ(pixels[i * 4 + 1], imageIndex == 0 && i % 8 >= 4 ? 255 : 0);
            EXPECT_EQ(pixels[i * 4 + 2], imageIndex == 1 ? 255 : 0);
            EXPECT_EQ(pixels[i * 4 + 3], 255);
        }
        buffer->UnMap();
    }
}

TEST_P(VulkanFrameCompilerFixture, DiscardedStoreEndsScopeAndCannotFeedLoad)
{
    NativeProbe       probe(*device);
    FrameGraphBuilder builder;
    auto              image = std::get<GraphImageHandle>(builder.Import(ImageOf()));
    FrameRasterPass   pass;
    pass.color = image;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    pass.load       = LoadOp::LOAD;
    pass.store      = StoreOp::DONT_CARE;
    pass.sideEffect = true;
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    pass.load  = LoadOp::CLEAR;
    pass.store = StoreOp::STORE;
    pass.clear = ClearValue{0.f, 1.f, 0.f, 1.f};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    auto buffer = Readback(builder, image);
    auto graph  = std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
    Execute(graph);
    if (GetParam() == VulkanFrameGraphMode::RENDER_PASS) {
        EXPECT_EQ(probe.subpassCounts, std::vector<uint32_t>({2, 1}));
        EXPECT_EQ(probe.scopeStores, std::vector<VkAttachmentStoreOp>({VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE}));
    } else
        EXPECT_EQ(probe.dynamicBegins, 3u);
    auto *pixels = static_cast<uint8_t *>(buffer->Map());
    ASSERT_NE(pixels, nullptr);
    for (size_t i = 0; i < 64; ++i) {
        EXPECT_EQ(pixels[i * 4], 0);
        EXPECT_EQ(pixels[i * 4 + 1], 255);
        EXPECT_EQ(pixels[i * 4 + 2], 0);
    }
    buffer->UnMap();
    auto data                                      = graph->GetData();
    std::get<FrameRasterPass>(data.passes[2]).load = LoadOp::LOAD;
    const auto lease                               = Begin();
    auto       rejected = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
    ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
    EXPECT_EQ(std::get<FrameError>(rejected).code, FrameErrorCode::COMPILE_FAILED);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(VulkanFrameCompilerFixture, LocalReadRequiresDeclarationAndBackendSupport)
{
    NativeProbe probe(*device);
    auto        graph   = ClearGraph();
    auto        data    = graph->GetData();
    auto        pass    = std::get<FrameRasterPass>(data.passes[0]);
    pass.load           = LoadOp::LOAD;
    pass.colorLocalRead = true;
    data.passes.emplace_back(pass);
    auto local    = std::make_shared<const FrozenFrameGraph>(graph->GetId(), data);
    auto analysis = AnalyzeFrameGraph(*local);
    ASSERT_TRUE(std::holds_alternative<FrameGraphAnalysis>(analysis));
    EXPECT_EQ(std::get<FrameGraphAnalysis>(analysis).livePasses, std::vector<uint32_t>({0, 1}));
    const auto lease    = Begin();
    auto       rejected = context->Compile(lease, local);
    ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
    EXPECT_EQ(std::get<FrameError>(rejected).code, FrameErrorCode::UNSUPPORTED);
    EXPECT_EQ(probe.pipelineAttempts, 0u);
    EXPECT_TRUE(probe.subpassCounts.empty());
    std::get<FrameRasterPass>(data.passes[1]).load = LoadOp::CLEAR;
    rejected = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
    ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
    EXPECT_EQ(std::get<FrameError>(rejected).code, FrameErrorCode::COMPILE_FAILED);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(VulkanFrameCompilerFixture, LaterPipelineFailureReleasesEarlierSubpasses)
{
    NativeProbe probe(*device);
    auto        graph    = ClearGraph();
    auto        data     = graph->GetData();
    auto        pass     = std::get<FrameRasterPass>(data.passes[0]);
    pass.load            = LoadOp::LOAD;
    pass.draws[0].shader = ShaderOf(true);
    data.passes.emplace_back(pass);
    data.passes.emplace_back(pass);
    probe.pipelineFailure     = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    probe.failPipelineAttempt = 2;
    const auto lease          = Begin();
    auto       rejected       = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
    ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
    EXPECT_EQ(std::get<FrameError>(rejected).cause.nativeCode, VK_ERROR_OUT_OF_DEVICE_MEMORY);
    EvictFrameCaches(*device);
    EXPECT_EQ(probe.pipelineAttempts, 2u);
    EXPECT_EQ(probe.pipelinesDestroyed, 1u);
    EXPECT_EQ(probe.renderPassesDestroyed, GetParam() == VulkanFrameGraphMode::RENDER_PASS ? 1u : 0u);
    EXPECT_EQ(probe.framebuffersDestroyed, GetParam() == VulkanFrameGraphMode::RENDER_PASS ? 1u : 0u);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(VulkanFrameCompilerFixture, CompileRejectsUndefinedContentsAndUnsupportedImportsWithoutBorrowingSlot)
{
    const auto graph  = ClearGraph();
    const auto lease  = Begin();
    auto       reject = [&](FrameGraphData data, FrameErrorCode expected) {
        auto result = context->Compile(lease, std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)));
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).code, expected);
    };
    auto data                                      = graph->GetData();
    std::get<FrameRasterPass>(data.passes[0]).load = LoadOp::LOAD;
    reject(std::move(data), FrameErrorCode::COMPILE_FAILED);
    data                                            = graph->GetData();
    std::get<FrameRasterPass>(data.passes[0]).store = StoreOp::DONT_CARE;
    reject(std::move(data), FrameErrorCode::COMPILE_FAILED);
    data                                = graph->GetData();
    data.images[0].descriptor.mipLevels = 2;
    reject(data, FrameErrorCode::COMPILE_FAILED);
    data.imageExports.clear();
    std::get<FrameRasterPass>(data.passes[0]).sideEffect = true;
    reject(std::move(data), FrameErrorCode::INVALID_ARGUMENT);
    data                                                    = graph->GetData();
    std::get<FrameRasterPass>(data.passes[0]).colorView.mip = 1;
    reject(std::move(data), FrameErrorCode::COMPILE_FAILED);
    data = graph->GetData();
    ++data.images[0].descriptor.extent.width;
    reject(std::move(data), FrameErrorCode::INVALID_ARGUMENT);
    data                                                       = graph->GetData();
    std::get<FrameRasterPass>(data.passes[0]).draws[0].scissor = {{7, 0}, {2, 8}};
    reject(std::move(data), FrameErrorCode::COMPILE_FAILED);
    data                                                               = graph->GetData();
    std::get<FrameRasterPass>(data.passes[0]).draws[0].blend.writeMask = 0xFF;
    reject(std::move(data), FrameErrorCode::COMPILE_FAILED);
    data                                                              = graph->GetData();
    std::get<FrameRasterPass>(data.passes[0]).draws[0].blend.srcColor = BlendFactor::SRC1_COLOR;
    reject(std::move(data), FrameErrorCode::UNSUPPORTED);
    data = graph->GetData();
    data.buffers.push_back({BufferOf(BufferUsageFlagBit::TRANSFER_DST), 256, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false});
    BufferImageCopy copy{};
    copy.subRange.layers = 1;
    copy.imageExtent     = {4, 8, 1};
    data.passes.emplace_back(BufferImageCopyPass{{graph->GetId(), 0}, {graph->GetId(), 0}, copy, ImageCopyDirection::IMAGE_TO_BUFFER});
    data.exports.push_back({{graph->GetId(), 0}, 0, 256});
    reject(std::move(data), FrameErrorCode::COMPILE_FAILED);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(VulkanFrameCompilerFixture, WrongCompilerRejectsPlanAndCancellationReleasesNativeObjects)
{
    NativeProbe                           probe(*device);
    const auto                            lease  = Begin();
    auto                                  graph  = ClearGraph();
    std::weak_ptr<const FrozenFrameGraph> weak   = graph;
    auto                                  result = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(result));
    auto       plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(result));
    const auto otherMode =
        GetParam() == VulkanFrameGraphMode::RENDER_PASS ? VulkanFrameGraphMode::DYNAMIC_RENDERING : VulkanFrameGraphMode::RENDER_PASS;
    std::unique_ptr<DeviceFrameContext> other(device->CreateFrameContext({2, 1, 92, 2, otherMode}));
    ASSERT_TRUE(other);
    EXPECT_TRUE(std::holds_alternative<FrameError>(other->Dispatch(plan)));
    ASSERT_TRUE(plan);
    EvictFrameCaches(*device);
    graph.reset();
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    const auto ticket = std::get<EncodingTicket>(dispatched);
    EXPECT_FALSE(weak.expired());
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->Cancel(ticket)));
    EXPECT_TRUE(weak.expired());
    EvictFrameCaches(*device);
    EXPECT_EQ(probe.pipelinesDestroyed, 1u);
    EvictFrameCaches(*device);
    EXPECT_EQ(probe.renderPassesDestroyed, GetParam() == VulkanFrameGraphMode::RENDER_PASS ? 1u : 0u);
    EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(context->TryCollect(ticket)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(VulkanFrameCompilerFixture, NativeCreationFailuresCleanUpAndPreserveErrorCodes)
{
    NativeProbe probe(*device);
    auto        graph = ClearGraph();
    const auto  lease = Begin();
    if (GetParam() == VulkanFrameGraphMode::RENDER_PASS) {
        probe.framebufferFailure = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        auto result              = context->Compile(lease, graph);
        ASSERT_TRUE(std::holds_alternative<FrameError>(result));
        EXPECT_EQ(std::get<FrameError>(result).cause.nativeCode, VK_ERROR_OUT_OF_DEVICE_MEMORY);
        EvictFrameCaches(*device);
        EXPECT_EQ(probe.renderPassesDestroyed, 1u);
        EXPECT_EQ(probe.framebuffersDestroyed, 0u);
        probe.framebufferFailure = VK_SUCCESS;
    }
    probe.pipelineFailure = VK_ERROR_OUT_OF_HOST_MEMORY;
    auto result           = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).cause.nativeCode, VK_ERROR_OUT_OF_HOST_MEMORY);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    probe.pipelineFailure = VK_SUCCESS;
    const auto next       = Begin();
    result                = context->Compile(next, graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(result));
    std::get<std::unique_ptr<BackendCompiledFrame>>(result).reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(next)));
}

TEST_P(VulkanFrameCompilerFixture, DeviceLostDuringCompilationClosesContext)
{
    NativeProbe probe(*device);
    auto        graph     = ClearGraph();
    const auto  lease     = Begin();
    probe.pipelineFailure = VK_ERROR_DEVICE_LOST;
    auto result           = context->Compile(lease, graph);
    ASSERT_TRUE(std::holds_alternative<FrameError>(result));
    EXPECT_EQ(std::get<FrameError>(result).cause.code, GpuErrorCode::DEVICE_LOST);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, graph)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

INSTANTIATE_TEST_SUITE_P(VulkanModes,
                         VulkanFrameCompilerFixture,
                         testing::Values(VulkanFrameGraphMode::DYNAMIC_RENDERING, VulkanFrameGraphMode::RENDER_PASS));
