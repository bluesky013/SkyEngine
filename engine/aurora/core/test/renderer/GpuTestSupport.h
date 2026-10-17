#pragma once

#include "prepare/ScenePrepareScheduler.h"
#include <VulkanCommandPool.h>
#include <VulkanDevice.h>
#include <array>
#include <aurora/rhi/Instance.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <gtest/gtest.h>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    std::weak_ptr<const void> TrackGraphGpuPayload(std::shared_ptr<const FrozenFrameGraph> &graph)
    {
        auto data                      = graph->GetData();
        data.payload                   = std::make_shared<std::shared_ptr<const void>>(data.payload);
        std::weak_ptr<const void> weak = data.payload;
        graph                          = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
        return weak;
    }
    struct Finally {
        std::function<void()> fn;
        ~Finally()
        {
            fn();
        }
    };
    FenceQueryResult Await(Fence &fence)
    {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        auto       result   = fence.Query();
        while (result.status == FenceStatus::PENDING && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
            result = fence.Query();
        }
        return result;
    }
    std::shared_ptr<const PrepareProducts> PreparePayload(uint32_t seed)
    {
        constexpr SceneId scene{0, 1};
        CpuSceneStore     store;
        FramePacket       packet;
        packet.sequence = 1;
        packet.commands.emplace_back(CreateScene{scene, {}});
        for (uint32_t i = 0; i < 2; ++i) {
            RenderEntityDesc object;
            object.world.v[12] = static_cast<float>(seed + i * 7);
            packet.commands.emplace_back(CreateRenderEntity{{scene, {i, 1}}, object});
        }
        if (!store.Apply(packet).Succeeded())
            return {};
        auto scheduler = ScenePrepareScheduler::Create({2});
        if (!scheduler.scheduler)
            return {};
        auto       frame = store.Freeze();
        SystemDesc system{1, "GpuProbePayload"};
        system.accesses = {ComponentAccess<RenderTransform>("scene", SceneAccessOwner(scene), AccessMode::READ)};
        auto submitted =
            scheduler.scheduler->Submit(*frame, {system},
                                        [scene](const FrozenPrepareContext &context) {
                                            context.Read<RenderTransform>(scene).ForEach([&](RenderEntityId, const RenderTransform &transform) {
                                                std::array<uint32_t, 16> values;
                                                for (uint32_t i = 0; i < values.size(); ++i)
                                                    values[i] = static_cast<uint32_t>(transform.world.v[12]) * 31 + i;
                                                EXPECT_TRUE(context.Emit(1, std::as_bytes(std::span{values})));
                                            });
                                        },
                                        {{1, scene, 1}}, {{1, 1, 64, 0}});
        if (!submitted.Accepted())
            return {};
        const auto &result = submitted.group->Wait();
        return result.Succeeded() ? result.products : nullptr;
    }
    void HostReadBarrier(Device *device, CommandBuffer *command)
    {
        if (device->GetAPI() != API::VULKAN)
            return;
        // Probe-only native host visibility; the future graph compiler must emit this dependency itself.
        VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask  = VK_PIPELINE_STAGE_2_HOST_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers    = &barrier;
        static_cast<VulkanDevice *>(device)->GetDeviceFn().vkCmdPipelineBarrier2(static_cast<VulkanCommandBuffer *>(command)->GetNativeHandle(),
                                                                                 &info);
    }
} // namespace
