
#include <rdg/VulkanDeviceFrameContext.h>

#include "VulkanFrameCompiler.h"
#include "VulkanGraphResources.h"
#include <VulkanCommandPool.h>
#include <VulkanDevice.h>
#include <VulkanShader.h>
#include <algorithm>
#include <core/async/TaskDispatcher.h>

namespace sky::aurora {
    namespace {
        struct VulkanEncodeWorker final : ThreadContext {
            explicit VulkanEncodeWorker(uint32_t index) : index(index)
            {
            }
            uint32_t index;
        };
        struct WorkerPartition {
            std::unique_ptr<VulkanCommandPool> pool;
            std::vector<VkCommandBuffer>       commands;
            uint64_t                           generation = 0;
            uint32_t                           issued     = 0;
        };
    } // namespace
    struct VulkanDeviceFrameContext::EncodingSession {
        EncodingTicket                         ticket;
        std::shared_ptr<VulkanCompiledFrame>   plan;
        TaskDispatcher                         dispatcher;
        std::future<DispatchState>             completion;
        std::vector<VkCommandBuffer>           fragments;
        std::vector<std::optional<FrameError>> errors;
        std::atomic_bool                       cancelled{false};
    };
    struct VulkanDeviceFrameContext::State {
        struct Descriptors {
            VkDescriptorPool             pool      = VK_NULL_HANDLE;
            uint32_t                     resources = 0, samplers = 0, sets = 0;
            std::vector<VkDescriptorSet> allocated;
        };
        FrameAllocationLimits                     limits;
        std::vector<Descriptors>                  descriptors;
        std::vector<std::vector<WorkerPartition>> partitions;
        std::unique_ptr<ThreadPool>               pool;
        std::unique_ptr<EncodingSession>          session;
        uint32_t                                  maxRecordings = 0;
        const BackendCompiledFrame               *issuedPlan    = nullptr;
        CompiledFrameIdentity                     issuedIdentity;

        bool Encode(VulkanDevice &device, EncodingSession &batch, uint32_t taskIndex, uint32_t worker)
        {
            if (batch.cancelled.load(std::memory_order_acquire))
                return true;
            const auto &lease     = batch.ticket.frame.lease;
            auto       &partition = partitions[lease.slot][worker];
            const auto &fn        = device.GetDeviceFn();
            auto        fail      = [&](VkResult status) {
                batch.errors[taskIndex] = VulkanFrameError(status, FrameErrorCode::ENCODE_FAILED);
                return false;
            };
            // Only this actual worker touches its partition; a new lease means the old GPU use retired.
            if (partition.generation != lease.generation) {
                const auto reset = fn.vkResetCommandPool(device.GetNativeHandle(), partition.pool->GetNativeHandle(), 0);
                if (reset != VK_SUCCESS)
                    return fail(reset);
                partition.generation = lease.generation;
                partition.issued     = 0;
            }
            const auto ordinal = partition.issued++;
            if (ordinal == partition.commands.size()) {
                VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocation.commandPool        = partition.pool->GetNativeHandle();
                allocation.level              = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
                allocation.commandBufferCount = 1;
                VkCommandBuffer command       = VK_NULL_HANDLE;
                const auto      status        = fn.vkAllocateCommandBuffers(device.GetNativeHandle(), &allocation, &command);
                if (status != VK_SUCCESS) {
                    --partition.issued;
                    return fail(status);
                }
                partition.commands.push_back(command);
            }
            const auto                              command = partition.commands[ordinal];
            const auto                             &task    = batch.plan->encodeTasks[taskIndex];
            const auto                             &pass    = batch.plan->rasterPasses[task.rasterPass];
            VkCommandBufferInheritanceInfo          inheritance{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
            VkCommandBufferInheritanceRenderingInfo rendering{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO};
            if (const auto *traditional = std::get_if<VulkanRenderPassInheritance>(&pass.inheritance))
                inheritance = traditional->GetInfo();
            else {
                rendering         = std::get<VulkanRenderingInheritance>(pass.inheritance).GetInfo();
                inheritance.pNext = &rendering;
            }
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            begin.pInheritanceInfo = &inheritance;
            const auto begun       = fn.vkBeginCommandBuffer(command, &begin);
            if (begun != VK_SUCCESS)
                return fail(begun);
            batch.plan->encoder->EncodeFragment(fn, command, pass, task);
            const auto ended = fn.vkEndCommandBuffer(command);
            if (ended != VK_SUCCESS)
                return fail(ended);
            batch.fragments[taskIndex] = command;
            return true;
        }
    };

    VulkanDeviceFrameContext::VulkanDeviceFrameContext(VulkanDevice *device) : device(device), state(std::make_unique<State>())
    {
    }
    VulkanDeviceFrameContext::~VulkanDeviceFrameContext()
    {
        if (state->pool) {
            if (state->session)
                state->session->cancelled.store(true, std::memory_order_release);
            state->pool->WaitIdle();
            state->session.reset();
        }
        // Native slot resources must outlive submitted work, including serial recording.
        if (!state->descriptors.empty())
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
        for (auto &partition : state->descriptors)
            if (partition.pool)
                device->GetDeviceFn().vkDestroyDescriptorPool(device->GetNativeHandle(), partition.pool, nullptr);
    }
    bool VulkanDeviceFrameContext::InitBackend(const DeviceFrameContextInitInfo &info)
    {
        state->limits = info.allocations;
        state->descriptors.resize(info.inflightNum);
        const auto &fn = device->GetDeviceFn();
        if (info.vulkanCompiler == VulkanFrameGraphMode::RENDER_PASS) {
            if (!fn.vkCreateRenderPass2 || !fn.vkDestroyRenderPass || !fn.vkCreateFramebuffer || !fn.vkDestroyFramebuffer ||
                !fn.vkCmdBeginRenderPass2 || !fn.vkCmdNextSubpass2 || !fn.vkCmdEndRenderPass2)
                return false;
            compiler = std::make_unique<VulkanRenderPassCompiler>(*device);
        } else {
            if (!fn.vkCmdBeginRendering || !fn.vkCmdEndRendering)
                return false;
            compiler = std::make_unique<VulkanDynamicRenderingCompiler>(*device);
        }
        state->maxRecordings = info.maxRecordingsPerSlot;
        if (info.parallelNum > 1) {
            if (!fn.vkCmdExecuteCommands)
                return false;
            state->partitions.resize(info.inflightNum);
            for (auto &slot : state->partitions) {
                slot.resize(info.parallelNum);
                for (auto &partition : slot) {
                    partition.pool = std::make_unique<VulkanCommandPool>(*device, device->GetQueueFamilyIndex(QueueType::GRAPHICS),
                                                                         VK_COMMAND_BUFFER_LEVEL_SECONDARY);
                    if (!partition.pool->Init())
                        return false;
                }
            }
            state->pool = std::make_unique<ThreadPool>(info.parallelNum, [](uint32_t index) { return new VulkanEncodeWorker(index); });
        }
        return true;
    }
    FrameBufferStorageResult VulkanDeviceFrameContext::CreateFrameBuffer(const Buffer::Descriptor &desc)
    {
        CounterPtr<VulkanBuffer> buffer(new VulkanBuffer(*device));
        if (!buffer->Init(desc))
            return VulkanFrameError(buffer->GetCreateResult(), FrameErrorCode::OUT_OF_MEMORY);
        uint8_t *mapped = nullptr;
        if (desc.memory != MemoryType::GPU_ONLY) {
            if (!buffer->IsHostCoherent())
                return FrameError{FrameErrorCode::UNSUPPORTED, "Frame buffers require host-coherent memory"};
            mapped = buffer->Map();
            if (!mapped)
                return VulkanFrameError(buffer->GetMapResult(), FrameErrorCode::INVALID_STATE);
        }
        return FrameBufferStorage{std::move(buffer), mapped};
    }
    FrameOperationResult VulkanDeviceFrameContext::ResetLeaseResources(uint32_t slot) noexcept
    {
        auto &partition = state->descriptors[slot];
        if (partition.pool) {
            const auto status = device->GetDeviceFn().vkResetDescriptorPool(device->GetNativeHandle(), partition.pool, 0);
            if (status != VK_SUCCESS)
                return VulkanFrameError(status, FrameErrorCode::INVALID_STATE);
        }
        partition.resources = partition.samplers = partition.sets = 0;
        partition.allocated.clear();
        return FrameOperationComplete{};
    }
    std::variant<FrameError, VkDescriptorSet>
    VulkanDeviceFrameContext::AllocateDescriptorSet(const DeviceFrameLease &lease, const Shader &shader, uint32_t set)
    {
        auto valid = ValidateCompileLease(lease);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        if (shader.GetDevice() != device)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        const auto &native = static_cast<const VulkanShader &>(shader);
        const auto  layout = native.GetDescriptorSetLayout(set);
        if (!layout)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        uint64_t resources = 0, samplers = 0;
        for (const auto &binding : native.GetReflection().resources)
            if (binding.set == set)
                (binding.type == ShaderResourceType::SAMPLER ? samplers : resources) += binding.count;
        auto       &partition = state->descriptors[lease.slot];
        const auto &limits    = state->limits;
        if (resources > limits.resourceDescriptors - partition.resources || samplers > limits.samplerDescriptors - partition.samplers ||
            partition.sets == limits.descriptorSets)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Vulkan descriptor budget exhausted (resources=" + std::to_string(partition.resources) +
                                                                 ", samplers=" + std::to_string(partition.samplers) +
                                                                 ", sets=" + std::to_string(partition.sets) + ")"};
        const auto &fn   = device->GetDeviceFn();
        auto        fail = [&](VkResult status) {
            auto error = VulkanFrameError(status, FrameErrorCode::COMPILE_FAILED);
            ObserveFailure(error);
            return error;
        };
        if (!partition.pool) {
            std::vector<VkDescriptorPoolSize> sizes;
            if (limits.samplerDescriptors)
                sizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, limits.samplerDescriptors});
            if (limits.resourceDescriptors)
                for (auto type : {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT})
                    sizes.push_back({type, limits.resourceDescriptors});
            VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            info.maxSets       = limits.descriptorSets;
            info.poolSizeCount = static_cast<uint32_t>(sizes.size());
            info.pPoolSizes    = sizes.data();
            const auto status  = fn.vkCreateDescriptorPool(device->GetNativeHandle(), &info, nullptr, &partition.pool);
            if (status != VK_SUCCESS)
                return fail(status);
        }
        VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        info.descriptorPool     = partition.pool;
        info.descriptorSetCount = 1;
        info.pSetLayouts        = &layout;
        VkDescriptorSet result  = VK_NULL_HANDLE;
        const auto      status  = fn.vkAllocateDescriptorSets(device->GetNativeHandle(), &info, &result);
        if (status != VK_SUCCESS)
            return fail(status);
        partition.resources += static_cast<uint32_t>(resources);
        partition.samplers += static_cast<uint32_t>(samplers);
        ++partition.sets;
        partition.allocated.push_back(result);
        return result;
    }

    FrameCompileResult VulkanDeviceFrameContext::Compile(const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph)
    {
        auto valid = ValidateCompileLease(lease);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        if (!graph)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        auto      &partition = state->descriptors[lease.slot];
        const auto resources = partition.resources, samplers = partition.samplers, sets = partition.sets;
        const auto cursor   = partition.allocated.size();
        auto       rollback = [&]() -> std::optional<FrameError> {
            if (partition.allocated.size() > cursor) {
                const auto status = device->GetDeviceFn().vkFreeDescriptorSets(device->GetNativeHandle(), partition.pool,
                                                                               static_cast<uint32_t>(partition.allocated.size() - cursor),
                                                                               partition.allocated.data() + cursor);
                if (status != VK_SUCCESS) {
                    auto error = VulkanFrameError(status, FrameErrorCode::COMPILE_FAILED);
                    ObserveFailure(error);
                    return error;
                }
            }
            partition.allocated.resize(cursor);
            partition.resources = resources;
            partition.samplers  = samplers;
            partition.sets      = sets;
            return {};
        };
        auto result = compiler->Compile(std::move(graph), *this, lease);
        if (auto *error = std::get_if<FrameError>(&result)) {
            ObserveFailure(*error);
            if (auto failure = rollback(); failure && error->cause.code != GpuErrorCode::DEVICE_LOST)
                return std::move(*failure);
            return std::move(*error);
        }
        if (std::holds_alternative<FrameNoWork>(result)) {
            if (auto failure = rollback())
                return std::move(*failure);
            return FrameNoWork{};
        }
        auto plan     = std::move(std::get<std::unique_ptr<VulkanCompiledFrame>>(result));
        auto reserved = ReservePlan(lease, *plan->graph, plan->resources);
        if (auto *error = std::get_if<FrameError>(&reserved)) {
            if (auto failure = rollback())
                return std::move(*failure);
            return std::move(*error);
        }
        plan->reservation     = std::move(std::get<FramePlanLease>(reserved));
        state->issuedPlan     = plan.get();
        state->issuedIdentity = plan->GetIdentity();
        return std::unique_ptr<BackendCompiledFrame>(std::move(plan));
    }
    FrameRecordResult VulkanDeviceFrameContext::RecordPrimary(EncodingSession &session)
    {
        auto &plan       = *session.plan;
        auto  allocation = AllocatePlanCommand(plan.reservation, 0);
        if (auto *error = std::get_if<FrameError>(&allocation))
            return std::move(*error);
        auto                    *rhi    = std::get<CommandBuffer *>(allocation);
        const auto               native = static_cast<VulkanCommandBuffer *>(rhi)->GetNativeHandle();
        const auto              &fn     = device->GetDeviceFn();
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags      = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        const auto begun = fn.vkBeginCommandBuffer(native, &begin);
        if (begun != VK_SUCCESS)
            return VulkanFrameError(begun, FrameErrorCode::ENCODE_FAILED);
        plan.encoder->Encode(fn, native, *rhi, session.fragments);
        const auto ended = fn.vkEndCommandBuffer(native);
        if (ended == VK_ERROR_DEVICE_LOST)
            return VulkanFrameError(ended, FrameErrorCode::ENCODE_FAILED);
        if (plan.encoder->encodeError)
            return std::move(*plan.encoder->encodeError);
        if (ended != VK_SUCCESS)
            return VulkanFrameError(ended, FrameErrorCode::ENCODE_FAILED);
        FrameRecordingInputs inputs;
        inputs.payload = std::make_shared<FrameGpuPayload<VulkanFrameEncoder>>(std::move(plan.resources), std::move(plan.encoder));
        inputs.receipt = {std::move(plan.graph), std::move(plan.livePasses)};
        return FinishPlanRecording(plan.reservation, std::move(inputs));
    }
    FrameDispatchResult VulkanDeviceFrameContext::Dispatch(std::unique_ptr<BackendCompiledFrame> &compiled)
    {
        if (!compiled || compiled.get() != state->issuedPlan || compiled->GetIdentity() != state->issuedIdentity)
            return FrameError{FrameErrorCode::WRONG_CONTEXT};
        auto *plan    = static_cast<VulkanCompiledFrame *>(compiled.get());
        auto  started = BeginEncoding(plan->reservation);
        if (std::holds_alternative<FrameError>(started))
            return started;
        auto batch    = std::make_unique<EncodingSession>();
        batch->ticket = std::get<EncodingTicket>(started);
        batch->plan.reset(static_cast<VulkanCompiledFrame *>(compiled.release()));
        state->issuedPlan = nullptr;
        const auto ticket = batch->ticket;
        if (!state->pool || plan->encodeTasks.empty()) {
            PublishEncoding(ticket, RecordPrimary(*batch));
            return ticket;
        }
        // Bound total fragments so a worker stealing the entire batch cannot exhaust its ordinal budget.
        const auto budget = std::min<uint64_t>(state->maxRecordings, uint64_t{state->pool->GetThreadCount()} * ThreadPool::GetLocalQueueCapacity());
        if (plan->encodeTasks.size() > budget) {
            PublishEncoding(ticket, FrameError{FrameErrorCode::OUT_OF_MEMORY, "Vulkan encode task/recording budget exceeded"});
            return ticket;
        }
        batch->fragments.resize(plan->encodeTasks.size(), VK_NULL_HANDLE);
        batch->errors.resize(plan->encodeTasks.size());
        for (uint32_t i = 0; i < plan->encodeTasks.size(); ++i) {
            batch->dispatcher.CreateCheckedTask([this, ptr = batch.get(), i](ThreadContext &thread) {
                return state->Encode(*device, *ptr, i, static_cast<VulkanEncodeWorker &>(thread).index);
            });
        }
        auto dispatch = batch->dispatcher.Submit(*state->pool);
        if (!dispatch.Accepted()) {
            PublishEncoding(ticket, FrameError{FrameErrorCode::ENCODE_FAILED, "Vulkan dispatcher rejected the batch"});
            return ticket;
        }
        batch->completion = std::move(dispatch.completion);
        state->session    = std::move(batch);
        return ticket;
    }
    FrameCollectResult VulkanDeviceFrameContext::TryCollect(const EncodingTicket &ticket)
    {
        if (state->session && state->session->ticket == ticket) {
            if (state->session->completion.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                return FramePending{};
            auto       session   = std::move(state->session);
            const auto completed = session->completion.get();
            if (auto *error = FindEncodingFailure(session->errors))
                PublishEncoding(ticket, std::move(*error));
            else if (completed != DispatchState::SUCCEEDED)
                PublishEncoding(ticket, FrameError{FrameErrorCode::ENCODE_FAILED});
            else if (session->cancelled.load(std::memory_order_acquire))
                PublishCancelledEncoding(ticket);
            else
                PublishEncoding(ticket, RecordPrimary(*session));
        }
        return BackendFrameContext::TryCollect(ticket);
    }
    FrameOperationResult VulkanDeviceFrameContext::Cancel(const EncodingTicket &ticket)
    {
        if (state->session && state->session->ticket == ticket) {
            state->session->cancelled.store(true, std::memory_order_release);
            return FrameOperationComplete{};
        }
        return BackendFrameContext::Cancel(ticket);
    }
} // namespace sky::aurora
