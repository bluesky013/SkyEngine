//
// Created on 2026/10/30.
//

#include "graph/FrameGraphBuilder.h"

#include <VulkanDevice.h>
#include <VulkanInstance.h>
#include <array>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Instance.h>
#include <chrono>
#include <core/util/DynamicModule.h>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace sky;
using namespace sky::aurora;

namespace {
    struct ConfigCase {
        bool requested;
        bool hideFeature;
        bool hideEntry;
    };

    class FeatureProbe {
    public:
        FeatureProbe(VulkanInstanceFunctions &functions, ConfigCase config)
            : functions(functions), config(config), query(functions.vkGetPhysicalDeviceFeatures2), lookup(functions.vkGetDeviceProcAddr)
        {
            active                                 = this;
            functions.vkGetPhysicalDeviceFeatures2 = Query;
            functions.vkGetDeviceProcAddr          = Lookup;
        }
        ~FeatureProbe()
        {
            functions.vkGetPhysicalDeviceFeatures2 = query;
            functions.vkGetDeviceProcAddr          = lookup;
            active                                 = nullptr;
        }

    private:
        static void VKAPI_PTR Query(VkPhysicalDevice device, VkPhysicalDeviceFeatures2 *features)
        {
            active->query(device, features);
            if (!active->config.hideFeature)
                return;
            for (void *entry = features->pNext; entry;) {
                const auto *header = static_cast<VkBaseOutStructure *>(entry);
                if (header->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES)
                    static_cast<VkPhysicalDeviceDynamicRenderingFeatures *>(entry)->dynamicRendering = VK_FALSE;
                entry = header->pNext;
            }
        }
        static PFN_vkVoidFunction VKAPI_PTR Lookup(VkDevice device, const char *name)
        {
            if (active->config.hideEntry && (std::strcmp(name, "vkCmdBeginRendering") == 0 || std::strcmp(name, "vkCmdEndRendering") == 0 ||
                                             std::strcmp(name, "vkCmdBeginRenderingKHR") == 0 || std::strcmp(name, "vkCmdEndRenderingKHR") == 0))
                return nullptr;
            return active->lookup(device, name);
        }

        VulkanInstanceFunctions         &functions;
        ConfigCase                       config;
        PFN_vkGetPhysicalDeviceFeatures2 query;
        PFN_vkGetDeviceProcAddr          lookup;
        static inline FeatureProbe      *active = nullptr;
    };

    class VulkanDeviceConfigTest : public testing::TestWithParam<ConfigCase> {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(library.Load());
            const auto create = library.GetAddress<Instance::Impl *(*)()>("CreateInstance");
            ASSERT_TRUE(create);
            backend.reset(create());
            ASSERT_TRUE(backend);
            Instance::Descriptor descriptor{"Vulkan configuration test", "SkyEngine", true, API::VULKAN};
            descriptor.vulkan.dynamicRendering = GetParam().requested;
            ASSERT_TRUE(backend->Init(descriptor));
            // The factory is owned by the explicitly loaded Vulkan backend.
            auto &instance = static_cast<VulkanInstance &>(*backend);
            probe          = std::make_unique<FeatureProbe>(const_cast<VulkanInstanceFunctions &>(instance.GetInstanceFn()), GetParam());
            device.reset(backend->CreateDevice());
            ASSERT_TRUE(device);
            auto &native = static_cast<VulkanDevice &>(*device);
            EXPECT_EQ(native.IsDynamicRenderingRequested(), GetParam().requested);
            EXPECT_EQ(native.IsDynamicRenderingEnabled(), GetParam().requested && native.IsDynamicRenderingSupported() && !GetParam().hideEntry);
            if (GetParam().hideFeature)
                EXPECT_FALSE(native.IsDynamicRenderingSupported());
            context.reset(device->CreateFrameContext({2, 2, 49}));
            ASSERT_TRUE(context);
        }
        void TearDown() override
        {
            if (device)
                device->WaitIdle();
            context.reset();
            device.reset();
            probe.reset();
            backend.reset();
        }

        DynamicModule                       library{"Aurora.Vulkan"};
        std::unique_ptr<Instance::Impl>     backend;
        std::unique_ptr<FeatureProbe>       probe;
        std::unique_ptr<Device>             device;
        std::unique_ptr<DeviceFrameContext> context;
    };
} // namespace

TEST_P(VulkanDeviceConfigTest, DeviceResolvedModeExecutesRasterAndReadback)
{
    const auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    const auto         lease = std::get<DeviceFrameLease>(begun);
    Buffer::Descriptor targetInfo;
    targetInfo.size   = 64;
    targetInfo.usage  = BufferUsageFlagBit::TRANSFER_DST;
    targetInfo.memory = MemoryType::GPU_TO_CPU;
    BufferPtr target(device->CreateBuffer(targetInfo));
    ASSERT_TRUE(target);
    FrameGraphBuilder builder;
    const auto        imported = builder.Import(target, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false);
    ASSERT_TRUE(std::holds_alternative<GraphBufferHandle>(imported));
    Image::Descriptor image;
    image.format         = PixelFormat::RGBA8_UNORM;
    image.extent         = {4, 4, 1};
    image.usage          = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    const auto transient = builder.CreateTransient(image);
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(transient));
    FrameRasterPass raster;
    raster.color = std::get<GraphImageHandle>(transient);
    raster.clear = {1.f, 0.f, 0.f, 1.f};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(raster)));
    BufferImageCopy region;
    region.imageExtent = image.extent;
    region.subRange    = {0, 0, 1, AspectFlagBit::COLOR_BIT};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(
        builder.Copy({std::get<GraphBufferHandle>(imported), raster.color, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({std::get<GraphBufferHandle>(imported), 0, targetInfo.size})));
    auto frozen = builder.Freeze();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
    auto compiled = context->Compile(lease, std::move(std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen)));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto       plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    std::unique_ptr<RecordedFrame> recorded;
    const auto                     deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!recorded && std::chrono::steady_clock::now() < deadline) {
        auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
        if (auto *ready = std::get_if<std::unique_ptr<RecordedFrame>>(&collected))
            recorded = std::move(*ready);
        else
            ASSERT_TRUE(std::holds_alternative<FramePending>(collected));
        std::this_thread::yield();
    }
    ASSERT_TRUE(recorded);
    const auto submitted = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
    std::array<FrameCompletion, 2> completions;
    size_t                         count = 0;
    while (!count && std::chrono::steady_clock::now() < deadline) {
        count = context->PollCompleted(completions);
        std::this_thread::yield();
    }
    ASSERT_EQ(count, 1u);
    ASSERT_EQ(completions[0].status, FrameCompletionStatus::COMPLETE);
    const auto *pixels = target->Map();
    ASSERT_TRUE(pixels);
    const std::array<uint8_t, 4> red{255, 0, 0, 255};
    for (uint32_t pixel = 0; pixel < 16; ++pixel)
        EXPECT_EQ(std::memcmp(pixels + pixel * 4, red.data(), red.size()), 0) << pixel;
    target->UnMap();

    if (!static_cast<VulkanDevice &>(*device).IsDynamicRenderingEnabled()) {
        std::unique_ptr<CommandPool> pool(device->CreateCommandPool(QueueType::GRAPHICS));
        ASSERT_TRUE(pool);
        // Command buffers allocated through this legacy pool are owned by the pool.
        auto *command = pool->Allocate();
        ASSERT_TRUE(command);
        EXPECT_EQ(command->CreateGraphicsEncoder(), nullptr);
    }
}

INSTANTIATE_TEST_SUITE_P(
    NativeVulkan,
    VulkanDeviceConfigTest,
    testing::Values(ConfigCase{true, false, false}, ConfigCase{false, false, false}, ConfigCase{true, true, false}, ConfigCase{true, false, true}));
