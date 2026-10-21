#pragma once

#include "graph/FrameGraphBuilder.h"
#include "prepare/ScenePrepareScheduler.h"
#include <VulkanCommandPool.h>
#include <VulkanDevice.h>
#include <array>
#include <aurora/rhi/DescriptorEncoder.h>
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
    ShaderReflection StorageReadReflection()
    {
        ShaderReflection reflection;
        ShaderResource   image;
        image.type              = ShaderResourceType::STORAGE_IMAGE;
        image.texture.dimension = ShaderTextureDimension::D2;
        reflection.resources.push_back(image);
        ShaderResource output;
        output.type    = ShaderResourceType::STORAGE_BUFFER;
        output.binding = 1;
        reflection.resources.push_back(output);
        ShaderResource input;
        input.type    = ShaderResourceType::STORAGE_BUFFER;
        input.binding = 2;
        reflection.resources.push_back(input);
        return reflection;
    }
    std::pair<std::shared_ptr<const FrozenFrameGraph>, BufferPtr>
    CustomStorageReadGraph(Device &device, ShaderPtr writer, ShaderPtr bufferWriter, const ShaderPtr &reader)
    {
        class Callback final : public FrameCustomPassEncoder {
        public:
            CounterPtr<ComputePipeline> pipeline;
            CounterPtr<ResourceGroup>   group;
            FrameOperationResult        Encode(FrameCustomPassContext &context) const noexcept override
            {
                auto encoder = context.command.CreateComputeEncoder();
                encoder->BindPipeline(pipeline.Get());
                encoder->BindResourceGroup(0, group.Get());
                encoder->Dispatch(4, 1, 1);
                return FrameOperationComplete{};
            }
        };
        FrameGraphImage image;
        image.descriptor.extent = {8, 8, 1};
        image.descriptor.format = PixelFormat::RGBA8_UNORM;
        image.descriptor.usage  = ImageUsageFlagBit::STORAGE;
        image.final             = ImageLayout::GENERAL;
        image.image             = ImagePtr(device.CreateImage(image.descriptor));
        Buffer::Descriptor desc;
        desc.size          = 16;
        desc.usage         = BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_SRC;
        auto output        = BufferPtr(device.CreateBuffer(desc));
        auto input         = BufferPtr(device.CreateBuffer(desc));
        desc.usage         = BufferUsageFlagBit::TRANSFER_DST;
        desc.memory        = MemoryType::GPU_TO_CPU;
        auto readback      = BufferPtr(device.CreateBuffer(desc));
        auto callback      = std::make_shared<Callback>();
        callback->pipeline = CounterPtr<ComputePipeline>(device.CreatePipelineState(ComputePipeline::Descriptor{reader.Get()}));
        callback->group    = CounterPtr<ResourceGroup>(device.CreateResourceGroup({reader.Get(), 0}));
        if (!image.image || !input || !output || !readback || !callback->pipeline || !callback->group) {
            ADD_FAILURE() << "Custom storage-read allocation failed";
            return {};
        }
        auto descriptor = callback->group->CreateEncoder();
        descriptor->WriteImage(0, image.image.Get(), ImageLayout::GENERAL, 0);
        descriptor->WriteBuffer(1, output.Get(), 0, 16, 0);
        descriptor->WriteBuffer(2, input.Get(), 0, 16, 0);
        descriptor->End();
        FrameGraphBuilder builder;
        const auto        source  = std::get<GraphImageHandle>(builder.Import(std::move(image)));
        const auto        storage = std::get<GraphBufferHandle>(builder.Import(output, CopyBufferState::COMMON, CopyBufferState::COMMON, false));
        const auto target      = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
        const auto inputHandle = std::get<GraphBufferHandle>(builder.Import(input, CopyBufferState::COMMON, CopyBufferState::COMMON, false));
        FrameComputePass writeBuffer;
        writeBuffer.shader  = std::move(bufferWriter);
        writeBuffer.groupsX = 4;
        writeBuffer.buffers.push_back({0, 0, {inputHandle, 0, 16}, 0, FrameAccessMode::WRITE});
        builder.Compute(std::move(writeBuffer));
        FrameComputePass write;
        write.shader  = std::move(writer);
        write.groupsX = write.groupsY = 8;
        write.images.push_back({0, 0, source, {}, FrameAccessMode::WRITE});
        builder.Compute(std::move(write));
        FrameCustomPass read;
        read.encoder = std::move(callback);
        read.images.push_back({0, 0, source, {}, FrameAccessMode::READ, FrameImageBindingKind::STORAGE});
        read.buffers.push_back({0, 1, {storage, 0, 16}, 0, FrameAccessMode::WRITE, FrameBufferBindingKind::STORAGE});
        read.buffers.push_back({0, 2, {inputHandle, 0, 16}, 0, FrameAccessMode::READ, FrameBufferBindingKind::STORAGE});
        builder.Custom(std::move(read));
        builder.Copy(BufferCopyPass{storage, target, 0, 0, 16});
        builder.Export({target, 0, 16});
        return {std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()), readback};
    }
    ShaderReflection ComputeUniformReflection(bool dynamic)
    {
        ShaderReflection reflection;
        for (const auto binding : {1u, 3u}) {
            ShaderResource resource;
            resource.type    = dynamic ? ShaderResourceType::UNIFORM_BUFFER_DYNAMIC : ShaderResourceType::UNIFORM_BUFFER;
            resource.binding = binding;
            reflection.resources.push_back(resource);
            ShaderBlockLayout block;
            block.binding = binding;
            block.size    = 16;
            reflection.blocks.push_back(block);
        }
        ShaderResource output;
        output.type = ShaderResourceType::STORAGE_BUFFER;
        output.set  = 1;
        reflection.resources.push_back(output);
        return reflection;
    }
    std::pair<std::shared_ptr<const FrozenFrameGraph>, BufferPtr> ComputeUniformGraph(Device &device, ShaderPtr shader, bool dynamic)
    {
        auto create = [&](uint64_t size, MemoryType memory, BufferUsageFlags usage) {
            Buffer::Descriptor descriptor;
            descriptor.size   = size;
            descriptor.memory = memory;
            descriptor.usage  = usage;
            return BufferPtr(device.CreateBuffer(descriptor));
        };
        auto upload   = create(1024, MemoryType::CPU_TO_GPU, BufferUsageFlagBit::TRANSFER_SRC);
        auto uniform  = create(1024, MemoryType::GPU_ONLY, BufferUsageFlagBit::UNIFORM | BufferUsageFlagBit::TRANSFER_DST);
        auto storage  = create(64, MemoryType::GPU_ONLY, BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_SRC);
        auto readback = create(64, MemoryType::GPU_TO_CPU, BufferUsageFlagBit::TRANSFER_DST);
        if (!upload || !uniform || !storage || !readback) {
            ADD_FAILURE() << "Compute uniform buffer allocation failed";
            return {};
        }
        std::array<uint32_t, 256> values{};
        values[64]   = 11;
        values[128]  = 7;
        auto *mapped = upload->Map();
        if (!mapped) {
            ADD_FAILURE() << "Compute uniform upload map failed";
            return {};
        }
        std::memcpy(mapped, values.data(), sizeof(values));
        upload->UnMap();
        FrameGraphBuilder builder;
        const auto source    = std::get<GraphBufferHandle>(builder.Import(upload, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true));
        const auto constants = std::get<GraphBufferHandle>(builder.Import(uniform, CopyBufferState::COMMON, CopyBufferState::SHADER_READ, false));
        const auto output = std::get<GraphBufferHandle>(builder.Import(storage, CopyBufferState::COMMON, CopyBufferState::UNORDERED_ACCESS, false));
        const auto host   = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, constants, 0, 0, 1024})));
        FrameComputePass pass;
        pass.shader  = std::move(shader);
        pass.groupsX = 4;
        for (const auto binding : {3u, 1u}) {
            const uint32_t offset = binding == 3 ? 512 : 256;
            pass.buffers.push_back({0, binding, {constants, dynamic ? 0u : offset, 16}, dynamic ? offset : 0, FrameAccessMode::READ});
        }
        pass.buffers.push_back({1, 0, {output, 0, 16}, 0, FrameAccessMode::WRITE});
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(pass))));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, host, 0, 0, 16})));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({host, 0, 16})));
        return {std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()), std::move(readback)};
    }
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
