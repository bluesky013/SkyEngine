//
// Created on 2026/10/17.
//

#include "frame/FrameScheduler.h"
#include "GpuTestSupport.h"
#include "graph/FrameGraphBuilder.h"

#include <D3D12Device.h>
#include <VulkanInstance.h>
#include <algorithm>
#include <d3d12sdklayers.h>
#include <windows.h>

namespace {
    struct VulkanSurfaceProbe {
        explicit VulkanSurfaceProbe(VulkanDevice &device)
            : functions(const_cast<VulkanInstanceFunctions &>(device.GetVulkanInstance().GetInstanceFn()))
            , saved(functions)
            , deviceFunctions(const_cast<VulkanDeviceFunctions &>(device.GetDeviceFn()))
            , savedDevice(deviceFunctions)
        {
            active                                              = this;
            functions.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = [](VkPhysicalDevice gpu, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *caps) {
                ++active->capsQueries;
                if (active->capsResult != VK_SUCCESS)
                    return active->capsResult;
                const auto result = active->saved.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, caps);
                if (result == VK_SUCCESS && active->changeCaps)
                    active->changeCaps(*caps);
                return result;
            };
            functions.vkGetPhysicalDeviceSurfaceFormatsKHR = [](VkPhysicalDevice gpu, VkSurfaceKHR surface, uint32_t *count,
                                                                VkSurfaceFormatKHR *formats) {
                ++active->formatQueries;
                if (active->formatResult != VK_SUCCESS)
                    return active->formatResult;
                const auto result = active->saved.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, count, formats);
                if (result == VK_SUCCESS && formats && active->formatIncomplete)
                    return VK_INCOMPLETE;
                if (result == VK_SUCCESS && formats && active->reverseFormats)
                    std::reverse(formats, formats + *count);
                if (result == VK_SUCCESS && formats && active->changeFormats)
                    active->changeFormats(*count, formats);
                return result;
            };
            functions.vkGetPhysicalDeviceSurfacePresentModesKHR = [](VkPhysicalDevice gpu, VkSurfaceKHR surface, uint32_t *count,
                                                                     VkPresentModeKHR *modes) {
                ++active->modeQueries;
                if (active->modeResult != VK_SUCCESS)
                    return active->modeResult;
                const auto result = active->saved.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, count, modes);
                if (result == VK_SUCCESS && modes && active->modeIncomplete)
                    return VK_INCOMPLETE;
                if (result == VK_SUCCESS && modes && active->changeModes)
                    active->changeModes(*count, modes);
                return result;
            };
            deviceFunctions.vkAcquireNextImageKHR = [](VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore,
                                                       VkFence fence, uint32_t *index) {
                if (active->acquireOutOfDate)
                    return VK_ERROR_OUT_OF_DATE_KHR;
                const auto result = active->savedDevice.vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, index);
                return result == VK_SUCCESS && active->acquireSuboptimal ? VK_SUBOPTIMAL_KHR : result;
            };
            deviceFunctions.vkQueuePresentKHR = [](VkQueue queue, const VkPresentInfoKHR *info) {
                const auto result = active->savedDevice.vkQueuePresentKHR(queue, info);
                return result == VK_SUCCESS && active->presentSuboptimal ? VK_SUBOPTIMAL_KHR : result;
            };
            deviceFunctions.vkCreateSwapchainKHR = [](VkDevice device, const VkSwapchainCreateInfoKHR *info, const VkAllocationCallbacks *allocator,
                                                      VkSwapchainKHR *swapchain) {
                const auto result = active->savedDevice.vkCreateSwapchainKHR(device, info, allocator, swapchain);
                if (result == VK_SUCCESS)
                    active->createdPresentMode = info->presentMode;
                return result;
            };
        }
        ~VulkanSurfaceProbe()
        {
            functions       = saved;
            deviceFunctions = savedDevice;
            active          = nullptr;
        }
        VulkanInstanceFunctions                              &functions;
        const VulkanInstanceFunctions                         saved;
        VulkanDeviceFunctions                                &deviceFunctions;
        const VulkanDeviceFunctions                           savedDevice;
        VkResult                                              capsResult   = VK_SUCCESS;
        VkResult                                              formatResult = VK_SUCCESS, modeResult = VK_SUCCESS;
        std::function<void(VkSurfaceCapabilitiesKHR &)>       changeCaps;
        std::function<void(uint32_t &, VkSurfaceFormatKHR *)> changeFormats;
        std::function<void(uint32_t &, VkPresentModeKHR *)>   changeModes;
        bool                                                  reverseFormats    = false;
        bool                                                  acquireSuboptimal = false, presentSuboptimal = false, acquireOutOfDate = false;
        bool                                                  formatIncomplete = false, modeIncomplete = false;
        uint32_t                                              capsQueries   = 0;
        uint32_t                                              formatQueries = 0, modeQueries = 0;
        VkPresentModeKHR                                      createdPresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
        static inline VulkanSurfaceProbe                     *active             = nullptr;
    };
    void PumpWindows()
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    struct OutputWindow {
        OutputWindow()
        {
            WNDCLASSW cls{};
            cls.lpfnWndProc   = DefWindowProcW;
            cls.hInstance     = GetModuleHandleW(nullptr);
            cls.lpszClassName = L"AuroraB11Output";
            RegisterClassW(&cls);
            handle = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, cls.lpszClassName, L"Aurora output test", WS_POPUP, 0, 0, 96, 64, nullptr,
                                     nullptr, cls.hInstance, nullptr);
            ShowWindow(handle, SW_SHOWNOACTIVATE);
            PumpWindows();
        }
        ~OutputWindow()
        {
            if (handle)
                DestroyWindow(handle);
        }
        void Resize(uint32_t width, uint32_t height)
        {
            SetWindowPos(handle, nullptr, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
            PumpWindows();
        }
        HWND handle = nullptr;
    };
    class ControlledSwapChain final : public SwapChain {
    public:
        explicit ControlledSwapChain(SwapChainPtr value) : native(std::move(value))
        {
        }
        Device *GetDevice() const noexcept override
        {
            return native->GetDevice();
        }
        SurfaceQueryResult QuerySurface() const noexcept override
        {
            ++queries;
            auto result = native->QuerySurface();
            if (surfaceQuery)
                surfaceQuery(result);
            return result;
        }
        SwapChainAcquireResult TryAcquireFrame(uint64_t timeout) noexcept override
        {
            ++acquires;
            EXPECT_LE(timeout, 1'000'000u);
            if (acquireFailure)
                return {*acquireFailure, {}};
            auto result = blockAcquire ? SwapChainAcquireResult{{PresentationStatus::BUSY}, {}} : native->TryAcquireFrame(timeout);
            if (result.status == PresentationStatus::READY && afterAcquire)
                std::exchange(afterAcquire, {})();
            return result;
        }
        PresentationResult TryPresentFrame(uint64_t serial, bool gpuComplete) noexcept override
        {
            ++presents;
            return blockPresent ? PresentationResult{PresentationStatus::BUSY} : native->TryPresentFrame(serial, gpuComplete);
        }
        FenceQueryResult RetireFrame(uint64_t serial, bool abandon) noexcept override
        {
            ++retires;
            return blockRetire ? FenceQueryResult{FenceStatus::PENDING} : native->RetireFrame(serial, abandon);
        }
        PresentationResult TryResize(uint32_t width, uint32_t height) noexcept override
        {
            ++resizes;
            if (resizeFailure)
                return *resizeFailure;
            return native->TryResize(width, height);
        }
        PresentationResult TryClose() noexcept override
        {
            ++closes;
            return native->TryClose();
        }
        uint32_t AcquireNextImage(sky::aurora::Semaphore *, Fence *, uint64_t) override
        {
            ADD_FAILURE();
            return INVALID_INDEX;
        }
        void Present(uint32_t, uint32_t, sky::aurora::Semaphore *const *) override
        {
            ADD_FAILURE();
        }
        void Resize(uint32_t, uint32_t) override
        {
            ADD_FAILURE();
        }
        Image *GetImage(uint32_t index) const override
        {
            return native->GetImage(index);
        }
        uint32_t GetImageCount() const override
        {
            return native->GetImageCount();
        }
        PixelFormat GetFormat() const override
        {
            return native->GetFormat();
        }
        Extent2D GetExtent() const override
        {
            return native->GetExtent();
        }
        SwapChainStatus GetStatus() const override
        {
            return native->GetStatus();
        }
        Extent2D GetSurfaceSize() const override
        {
            return native->GetSurfaceSize();
        }
        SwapChainPtr                              native;
        bool                                      blockAcquire = false, blockPresent = false, blockRetire = false;
        uint32_t                                  acquires = 0, presents = 0, retires = 0, resizes = 0, closes = 0;
        std::function<void()>                     afterAcquire;
        std::function<void(SurfaceQueryResult &)> surfaceQuery;
        mutable uint32_t                          queries = 0;
        std::optional<PresentationResult>         acquireFailure;
        std::optional<PresentationResult>         resizeFailure;
    };
    class FrameOutputs : public testing::TestWithParam<uint32_t> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"FrameOutputTest", "SkyEngine", true, GetParam() == 2 ? API::DX12 : API::VULKAN});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            if (GetParam() == 2) {
                ASSERT_TRUE(SUCCEEDED(static_cast<D3D12Device *>(device)->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
            DeviceFrameContextInitInfo info;
            info.parallelNum    = 3;
            info.deviceEpoch    = 211;
            info.vulkanCompiler = GetParam() == 1 ? VulkanFrameGraphMode::RENDER_PASS : VulkanFrameGraphMode::DYNAMIC_RENDERING;
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
            scheduler = std::make_unique<FrameScheduler>(*device, *context);
        }
        void TearDown() override
        {
            for (const auto &output : outputs)
                output->blockAcquire = output->blockPresent = output->blockRetire = false;
            scheduler->RequestShutdown();
            if (!terminalTeardown)
                EXPECT_TRUE(Until([&] { return scheduler->Empty(); }));
            scheduler.reset();
            context.reset();
            outputs.clear();
            windows.clear();
            if (debug) {
                for (UINT64 i = 0; i < debug->GetNumStoredMessages(); ++i) {
                    SIZE_T size = 0;
                    debug->GetMessage(i, nullptr, &size);
                    std::vector<char> bytes(size);
                    auto             *message = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
                    if (SUCCEEDED(debug->GetMessage(i, message, &size)))
                        EXPECT_GT(message->Severity, D3D12_MESSAGE_SEVERITY_ERROR) << message->pDescription;
                }
            }
            debug.Reset();
            Instance::Destroy();
        }
        ControlledSwapChain *Add(ViewportId id)
        {
            windows.push_back(std::make_unique<OutputWindow>());
            SwapChain::Descriptor descriptor;
            descriptor.window          = windows.back()->handle;
            descriptor.width           = 96;
            descriptor.height          = 64;
            descriptor.preferredFormat = PixelFormat::BGRA8_UNORM;
            SwapChainPtr native(device->CreateSwapChain(descriptor));
            EXPECT_TRUE(native);
            if (!native)
                return nullptr;
            CounterPtr<ControlledSwapChain> output(new ControlledSwapChain(std::move(native)));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(scheduler->AddOutput(id, SwapChainPtr(output.Get()))));
            outputs.push_back(output);
            return output.Get();
        }
        void Pump()
        {
            PumpWindows();
            std::array<FrameCompletion, 4> completed;
            const auto                     count = context->PollCompleted(completed);
            scheduler->OnCompleted(std::span(completed).first(count));
            completedCount += static_cast<uint32_t>(count);
            std::array<OutputProgress, 64> progress;
            const auto                     events = scheduler->Pump(progress);
            for (size_t i = 0; i < events; ++i) {
                EXPECT_NE(progress[i].kind, OutputProgressKind::FAILED) << progress[i].error.nativeCode;
                if (progress[i].kind == OutputProgressKind::CLOSED)
                    closed.push_back(progress[i].viewport);
            }
        }
        bool Until(const std::function<bool()> &predicate)
        {
            const auto deadline = std::chrono::steady_clock::now() + 8s;
            do {
                Pump();
                if (predicate())
                    return true;
                std::this_thread::sleep_for(1ms);
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        }
        DeviceFrameLease Begin()
        {
            BeginSlotResult result;
            EXPECT_TRUE(Until([&] {
                result = context->TryBeginSlot();
                return !std::holds_alternative<FrameBusy>(result);
            }));
            EXPECT_TRUE(std::holds_alternative<DeviceFrameLease>(result));
            return std::holds_alternative<DeviceFrameLease>(result) ? std::get<DeviceFrameLease>(result) : DeviceFrameLease{};
        }
        std::unique_ptr<RecordedFrame> Record(DeviceFrameLease lease, std::shared_ptr<const FrozenFrameGraph> graph)
        {
            auto compiled = context->Compile(lease, std::move(graph));
            if (const auto *error = std::get_if<FrameError>(&compiled)) {
                ADD_FAILURE() << error->message << " native=" << error->cause.nativeCode;
                return {};
            }
            if (!std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled))
                return {};
            auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
            auto dispatched = context->Dispatch(plan);
            if (!std::holds_alternative<EncodingTicket>(dispatched)) {
                ADD_FAILURE();
                return {};
            }
            const auto         ticket = std::get<EncodingTicket>(dispatched);
            FrameCollectResult result;
            EXPECT_TRUE(Until([&] {
                result = context->TryCollect(ticket);
                return !std::holds_alternative<FramePending>(result);
            }));
            if (const auto *error = std::get_if<EncodingFailed>(&result))
                ADD_FAILURE() << error->error.message;
            if (!std::holds_alternative<std::unique_ptr<RecordedFrame>>(result))
                return {};
            return std::move(std::get<std::unique_ptr<RecordedFrame>>(result));
        }
        std::shared_ptr<const FrozenFrameGraph> Graph(OutputAcquireBatch batch)
        {
            FrameGraphBuilder builder;
            for (auto &output : batch.outputs) {
                const auto imported = builder.Import(std::move(output.image));
                if (!std::holds_alternative<GraphImageHandle>(imported)) {
                    ADD_FAILURE();
                    return {};
                }
                const auto      image = std::get<GraphImageHandle>(imported);
                FrameRasterPass pass;
                pass.color = image;
                pass.clear = {0.25f, 0.5f, 0.75f, 1.f};
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(pass))));
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(image)));
            }
            auto frozen = builder.Freeze();
            if (!std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen)) {
                ADD_FAILURE();
                return {};
            }
            return std::move(std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen));
        }
        size_t Render(std::vector<OutputSnapshot> snapshots)
        {
            const auto lease    = Begin();
            auto       acquired = scheduler->Acquire(lease, snapshots);
            if (!std::holds_alternative<OutputAcquireBatch>(acquired)) {
                ADD_FAILURE();
                return 0;
            }
            auto       batch = std::move(std::get<OutputAcquireBatch>(acquired));
            const auto count = batch.outputs.size();
            if (!count) {
                context->DiscardUnsubmitted(lease);
                return 0;
            }
            auto recorded = Record(lease, Graph(std::move(batch)));
            if (!recorded) {
                scheduler->Abandon(lease);
                context->DiscardUnsubmitted(lease);
                return 0;
            }
            const auto submitted = scheduler->Submit(recorded);
            EXPECT_TRUE(std::holds_alternative<SubmissionAccepted>(submitted));
            if (std::holds_alternative<SubmissionRejected>(submitted)) {
                recorded.reset();
                scheduler->Abandon(lease);
                context->DiscardUnsubmitted(lease);
                return 0;
            }
            return count;
        }
        void VerifySkippedOutputSubmission(bool skipAll)
        {
            auto *first  = Add({0, 1});
            auto *second = Add({1, 1});
            ASSERT_TRUE(first && second);
            first->acquireFailure = PresentationResult{PresentationStatus::OUT_OF_DATE};
            if (skipAll)
                second->acquireFailure = first->acquireFailure;
            const auto snapshots = scheduler->Snapshot();
            const auto lease     = Begin();

            Buffer::Descriptor sourceInfo;
            sourceInfo.size   = 16;
            sourceInfo.usage  = BufferUsageFlagBit::TRANSFER_SRC;
            sourceInfo.memory = MemoryType::CPU_TO_GPU;
            BufferPtr source(device->CreateBuffer(sourceInfo));
            auto      targetInfo = sourceInfo;
            targetInfo.size      = 272;
            targetInfo.usage     = BufferUsageFlagBit::TRANSFER_DST;
            targetInfo.memory    = MemoryType::GPU_TO_CPU;
            BufferPtr target(device->CreateBuffer(targetInfo));
            ASSERT_TRUE(source && target);
            const std::array<uint32_t, 4> expected{11, 22, 33, 44};
            auto                         *mappedSource = source->Map();
            ASSERT_TRUE(mappedSource);
            std::memcpy(mappedSource, expected.data(), sizeof(expected));
            source->UnMap();

            FrameGraphBuilder builder;
            const auto src = std::get<GraphBufferHandle>(builder.Import(source, CopyBufferState::HOST_WRITE, CopyBufferState::COPY_SOURCE, true));
            const auto dst = std::get<GraphBufferHandle>(builder.Import(target, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({src, dst, 0, 0, 16})));
            Image::Descriptor imageInfo;
            imageInfo.format     = PixelFormat::RGBA8_UNORM;
            imageInfo.extent     = {8, 8, 1};
            imageInfo.usage      = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
            const auto transient = builder.CreateTransient(imageInfo);
            ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(transient));
            FrameRasterPass offscreen;
            offscreen.color = std::get<GraphImageHandle>(transient);
            offscreen.clear = {1.f, 0.f, 0.f, 1.f};
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(offscreen)));
            BufferImageCopy region;
            region.bufferOffset = 16;
            region.imageExtent  = imageInfo.extent;
            region.subRange     = {0, 0, 1, AspectFlagBit::COLOR_BIT};
            ASSERT_TRUE(
                std::holds_alternative<FrameOperationComplete>(builder.Copy({dst, offscreen.color, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
            ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({dst, 0, targetInfo.size})));

            auto acquired = scheduler->Acquire(lease, snapshots);
            ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
            auto batch = std::move(std::get<OutputAcquireBatch>(acquired));
            ASSERT_EQ(batch.outputs.size(), skipAll ? 0u : 1u);
            ASSERT_EQ(batch.skipped.size(), skipAll ? 2u : 1u);
            for (const auto &skipped : batch.skipped)
                EXPECT_EQ(skipped.kind, OutputProgressKind::SKIPPED);
            for (auto &output : batch.outputs) {
                const auto imported = builder.Import(std::move(output.image));
                ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(imported));
                FrameRasterPass pass;
                pass.color = std::get<GraphImageHandle>(imported);
                pass.clear = {0.f, 1.f, 0.f, 1.f};
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
                ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(pass.color)));
            }
            auto frozen = builder.Freeze();
            ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
            auto graph    = std::move(std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen));
            auto recorded = Record(lease, std::move(graph));
            ASSERT_TRUE(recorded);
            ASSERT_TRUE(recorded->GetGraphReceipt());
            const std::vector<uint32_t> expectedPasses = skipAll ? std::vector<uint32_t>{0, 1, 2} : std::vector<uint32_t>{0, 1, 2, 3};
            EXPECT_EQ(recorded->GetGraphReceipt()->passes, expectedPasses);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(scheduler->Submit(recorded)));
            scheduler->RequestClose({0, 1});
            scheduler->RequestClose({1, 1});
            ASSERT_TRUE(Until([&] { return completedCount == 1 && closed.size() == 2; }));
            EXPECT_EQ(first->acquires, 1u);
            EXPECT_EQ(second->acquires, 1u);
            EXPECT_EQ(first->presents, 0u);
            EXPECT_EQ(first->retires, 0u);
            if (skipAll) {
                EXPECT_EQ(second->presents, 0u);
                EXPECT_EQ(second->retires, 0u);
            } else {
                EXPECT_GT(second->presents, 0u);
                EXPECT_GT(second->retires, 0u);
            }
            const auto *readback = static_cast<const uint8_t *>(target->Map());
            ASSERT_TRUE(readback);
            EXPECT_EQ(std::memcmp(readback, expected.data(), sizeof(expected)), 0);
            const std::array<uint8_t, 4> red{255, 0, 0, 255};
            for (uint32_t pixel = 0; pixel < 64; ++pixel)
                EXPECT_EQ(std::memcmp(readback + 16 + pixel * 4, red.data(), red.size()), 0) << pixel;
            target->UnMap();
        }
        Device                                      *device = nullptr;
        std::unique_ptr<DeviceFrameContext>          context;
        std::unique_ptr<FrameScheduler>              scheduler;
        std::vector<std::unique_ptr<OutputWindow>>   windows;
        std::vector<CounterPtr<ControlledSwapChain>> outputs;
        std::vector<ViewportId>                      closed;
        uint32_t                                     completedCount   = 0;
        bool                                         terminalTeardown = false;
        Microsoft::WRL::ComPtr<ID3D12InfoQueue>      debug;
    };
} // namespace

TEST_P(FrameOutputs, BeginViewportQueriesWithoutAcquiringAndDetectsNativeResize)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    auto prepared = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(prepared));
    const auto original = std::get<OutputSnapshot>(prepared);
    const auto queries  = output->queries;
    for (uint32_t i = 0; i < 3; ++i) {
        prepared = scheduler->BeginViewport({0, 1});
        ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(prepared));
        EXPECT_EQ(std::get<OutputSnapshot>(prepared).revision, original.revision);
    }
    EXPECT_EQ(output->queries, queries + 3);
    EXPECT_EQ(output->acquires, 0u);
    windows[0]->Resize(128, 80);
    EXPECT_TRUE(std::holds_alternative<OutputProgress>(scheduler->BeginViewport({0, 1})));
    EXPECT_EQ(Render({original}), 0u);
    ASSERT_TRUE(Until([&] { return output->resizes == 1; }));
    prepared = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(prepared));
    const auto updated = std::get<OutputSnapshot>(prepared);
    EXPECT_EQ(updated.extent.width, 128u);
    EXPECT_EQ(updated.extent.height, 80u);
    EXPECT_GT(updated.surfaceRevision, original.surfaceRevision);
    EXPECT_GT(updated.revision, original.revision);
    EXPECT_EQ(Render({updated}), 1u);
}

TEST_P(FrameOutputs, SurfaceQueryFailureAndBusyRecoverWithoutCancellingOtherOutput)
{
    auto *first = Add({0, 1});
    ASSERT_TRUE(first && Add({1, 1}));
    first->surfaceQuery = [](auto &result) { result = {{PresentationStatus::FAILED, {GpuErrorCode::NATIVE_ERROR, -710}}, {}}; };
    auto prepared       = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputProgress>(prepared));
    EXPECT_EQ(std::get<OutputProgress>(prepared).kind, OutputProgressKind::FAILED);
    EXPECT_EQ(std::get<OutputProgress>(prepared).error.nativeCode, -710);
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    first->surfaceQuery = [](auto &result) { result = {{PresentationStatus::BUSY}, {}}; };
    prepared            = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputProgress>(prepared));
    EXPECT_EQ(std::get<OutputProgress>(prepared).kind, OutputProgressKind::SKIPPED);
    EXPECT_EQ(first->acquires, 0u);
    EXPECT_EQ(first->resizes, 0u);
    first->surfaceQuery = {};
    prepared            = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(prepared));
    EXPECT_EQ(Render({std::get<OutputSnapshot>(prepared)}), 1u);
    EXPECT_TRUE(Until([&] { return completedCount == 2; }));
}

TEST_P(FrameOutputs, TransformOnlyChangeWaitsForRetirementAndInvalidatesOldPrepare)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    const auto original = std::get<OutputSnapshot>(scheduler->BeginViewport({0, 1}));
    output->blockRetire = true;
    EXPECT_EQ(Render({original}), 1u);
    ASSERT_TRUE(Until([&] { return completedCount == 1 && output->presents > 0; }));
    output->surfaceQuery = [revision = original.surfaceRevision](auto &result) {
        result.configuration.transform = SurfaceTransform::ROTATE_90;
        if (result.configuration.revision == revision)
            result.status = PresentationStatus::OUT_OF_DATE;
    };
    EXPECT_TRUE(std::holds_alternative<OutputProgress>(scheduler->BeginViewport({0, 1})));
    const auto changed = scheduler->Snapshot().front();
    EXPECT_GT(changed.revision, original.revision);
    EXPECT_EQ(changed.extent.width, original.extent.width);
    EXPECT_EQ(changed.transform, SurfaceTransform::ROTATE_90);
    Pump();
    EXPECT_EQ(output->resizes, 0u);
    output->blockRetire = false;
    ASSERT_TRUE(Until([&] { return output->resizes == 1; }));
    auto prepared = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(prepared));
    EXPECT_EQ(std::get<OutputSnapshot>(prepared).transform, SurfaceTransform::ROTATE_90);
    EXPECT_EQ(Render({original}), 0u);
    EXPECT_EQ(Render({std::get<OutputSnapshot>(prepared)}), 1u);
    output->surfaceQuery = {};
}

TEST_P(FrameOutputs, B13FeatureOwnerSurvivesGpuCompletionUntilPresentationRetires)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    output->blockRetire = true;
    const auto snapshot = std::get<OutputSnapshot>(scheduler->BeginViewport({0, 1}));
    const auto lease    = Begin();
    auto       acquired = scheduler->Acquire(lease, std::span(&snapshot, 1));
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto               owner    = std::make_shared<int>(1);
    std::weak_ptr<int> lifetime = owner;
    scheduler->Retain(lease, std::move(owner));
    auto graph    = Graph(std::move(std::get<OutputAcquireBatch>(acquired)));
    auto recorded = Record(lease, std::move(graph));
    ASSERT_TRUE(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(scheduler->Submit(recorded)));
    ASSERT_TRUE(Until([&] { return completedCount == 1 && output->retires > 0; }));
    EXPECT_FALSE(lifetime.expired());
    output->blockRetire = false;
    EXPECT_TRUE(Until([&] { return lifetime.expired(); }));
}

TEST_P(FrameOutputs, B13UnusedAcquiredOutputCanRetireWhileAnotherOutputSubmits)
{
    auto *unused = Add({0, 1});
    auto *used   = Add({1, 1});
    ASSERT_TRUE(unused && used);
    const auto lease    = Begin();
    auto       acquired = scheduler->Acquire(lease, scheduler->Snapshot());
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto batch = std::move(std::get<OutputAcquireBatch>(acquired));
    ASSERT_EQ(batch.outputs.size(), 2u);
    FrameGraphBuilder builder;
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(builder.Import(std::move(batch.outputs[0].image))));
    auto target = builder.Import(std::move(batch.outputs[1].image));
    ASSERT_TRUE(std::holds_alternative<GraphImageHandle>(target));
    FrameRasterPass pass;
    pass.color = std::get<GraphImageHandle>(target);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(pass)));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export(pass.color)));
    scheduler->AbandonOutput(lease, {0, 1});
    auto graph    = builder.Freeze();
    auto recorded = Record(lease, std::move(std::get<std::shared_ptr<const FrozenFrameGraph>>(graph)));
    ASSERT_TRUE(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(scheduler->Submit(recorded)));
    EXPECT_TRUE(Until([&] { return completedCount == 1 && unused->retires > 0 && used->presents > 0; }));
    EXPECT_EQ(unused->presents, 0u);
}

TEST_P(FrameOutputs, TransformChangesDuringAcquireAbandonOnlyThatOutput)
{
    auto *first = Add({0, 1});
    ASSERT_TRUE(first && Add({1, 1}));
    const auto snapshots = scheduler->Snapshot();
    first->afterAcquire  = [first] { first->surfaceQuery = [](auto &result) { result.configuration.transform = SurfaceTransform::ROTATE_270; }; };
    EXPECT_EQ(Render(snapshots), 1u);
    ASSERT_TRUE(Until([&] { return first->retires > 0 && completedCount == 1; }));
    EXPECT_EQ(first->presents, 0u);
    const auto prepared = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(prepared));
    EXPECT_EQ(std::get<OutputSnapshot>(prepared).transform, SurfaceTransform::ROTATE_270);
    first->surfaceQuery = {};
}

TEST_P(FrameOutputs, QueryErrorAfterAcquirePreservesCauseAndReleasesBorrowedOutput)
{
    auto *first = Add({0, 1});
    ASSERT_TRUE(first && Add({1, 1}));
    first->afterAcquire = [first] {
        first->surfaceQuery = [](auto &result) { result = {{PresentationStatus::FAILED, {GpuErrorCode::NATIVE_ERROR, -712}}, {}}; };
    };
    const auto lease    = Begin();
    auto       acquired = scheduler->Acquire(lease, scheduler->Snapshot());
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto batch = std::move(std::get<OutputAcquireBatch>(acquired));
    ASSERT_EQ(batch.outputs.size(), 1u);
    ASSERT_EQ(batch.skipped.size(), 1u);
    EXPECT_EQ(batch.skipped.front().kind, OutputProgressKind::FAILED);
    EXPECT_EQ(batch.skipped.front().error.nativeCode, -712);
    auto recorded = Record(lease, Graph(std::move(batch)));
    ASSERT_TRUE(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(scheduler->Submit(recorded)));
    first->surfaceQuery = {};
    ASSERT_TRUE(Until([&] { return completedCount == 1 && first->retires > 0; }));
    EXPECT_EQ(first->presents, 0u);
    const auto restored = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(restored));
    EXPECT_EQ(Render({std::get<OutputSnapshot>(restored)}), 1u);
}

TEST_P(FrameOutputs, OutOfDateRequeryFailureIsReportedBeforeOtherOutputSubmission)
{
    auto *first = Add({0, 1});
    ASSERT_TRUE(first && Add({1, 1}));
    first->acquireFailure = PresentationResult{PresentationStatus::OUT_OF_DATE};
    first->surfaceQuery   = [](auto &result) { result = {{PresentationStatus::FAILED, {GpuErrorCode::NATIVE_ERROR, -713}}, {}}; };
    const auto lease      = Begin();
    auto       acquired   = scheduler->Acquire(lease, scheduler->Snapshot());
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto batch = std::move(std::get<OutputAcquireBatch>(acquired));
    ASSERT_EQ(batch.outputs.size(), 1u);
    ASSERT_EQ(batch.skipped.size(), 1u);
    EXPECT_EQ(batch.skipped.front().kind, OutputProgressKind::FAILED);
    EXPECT_EQ(batch.skipped.front().error.nativeCode, -713);
    first->surfaceQuery = {};
    first->acquireFailure.reset();
    auto recorded = Record(lease, Graph(std::move(batch)));
    ASSERT_TRUE(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(scheduler->Submit(recorded)));
    ASSERT_TRUE(Until([&] { return completedCount == 1; }));
    EXPECT_TRUE(std::holds_alternative<OutputSnapshot>(scheduler->BeginViewport({0, 1})));
}

TEST_P(FrameOutputs, DeviceLostDuringAcquireAbandonsEarlierOutputAndStopsNewWork)
{
    auto *first  = Add({0, 1});
    auto *second = Add({1, 1});
    ASSERT_TRUE(first && second);
    second->acquireFailure = PresentationResult{PresentationStatus::FAILED, {GpuErrorCode::DEVICE_LOST, -714}};
    const auto lease       = Begin();
    const auto acquired    = scheduler->Acquire(lease, scheduler->Snapshot());
    ASSERT_TRUE(std::holds_alternative<FrameError>(acquired));
    EXPECT_EQ(std::get<FrameError>(acquired).code, FrameErrorCode::DEVICE_LOST);
    EXPECT_EQ(std::get<FrameError>(acquired).cause.nativeCode, -714);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    ASSERT_TRUE(Until([&] { return first->retires > 0; }));
    EXPECT_EQ(first->presents, 0u);
    EXPECT_EQ(completedCount, 0u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(scheduler->Acquire(lease, scheduler->Snapshot())));
    const auto stopped = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputProgress>(stopped));
    EXPECT_EQ(std::get<OutputProgress>(stopped).error.code, GpuErrorCode::DEVICE_LOST);
}

TEST_P(FrameOutputs, DeviceLostQueryRejectsAlreadyRecordedSubmission)
{
    auto *first  = Add({0, 1});
    auto *second = Add({1, 1});
    ASSERT_TRUE(first && second);
    const auto snapshots = scheduler->Snapshot();
    const auto lease     = Begin();
    auto       acquired  = scheduler->Acquire(lease, std::span(snapshots).first(1));
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto recorded = Record(lease, Graph(std::move(std::get<OutputAcquireBatch>(acquired))));
    ASSERT_TRUE(recorded);
    second->surfaceQuery = [](auto &result) { result = {{PresentationStatus::FAILED, {GpuErrorCode::DEVICE_LOST, -715}}, {}}; };
    const auto prepared  = scheduler->BeginViewport({1, 1});
    ASSERT_TRUE(std::holds_alternative<OutputProgress>(prepared));
    const auto submitted = scheduler->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionRejected>(submitted));
    EXPECT_EQ(std::get<SubmissionRejected>(submitted).error.cause.nativeCode, -715);
    EXPECT_TRUE(recorded);
    recorded.reset();
    scheduler->Abandon(lease);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    ASSERT_TRUE(Until([&] { return first->retires > 0; }));
    EXPECT_EQ(first->presents, 0u);
    EXPECT_EQ(completedCount, 0u);
}

TEST_P(FrameOutputs, SurfaceLostReportsThenHostCanReplaceClosedOutput)
{
    auto *first = Add({0, 1});
    ASSERT_TRUE(first);
    first->surfaceQuery = [](auto &result) { result = {{PresentationStatus::SURFACE_LOST, {GpuErrorCode::NATIVE_ERROR, -711}}, {}}; };
    const auto prepared = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputProgress>(prepared));
    EXPECT_EQ(std::get<OutputProgress>(prepared).error.nativeCode, -711);
    scheduler->RequestClose({0, 1});
    ASSERT_TRUE(Until([&] { return closed.size() == 1; }));
    ASSERT_TRUE(Add({0, 2}));
    const auto restored = scheduler->BeginViewport({0, 2});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(restored));
    EXPECT_EQ(Render({std::get<OutputSnapshot>(restored)}), 1u);
    EXPECT_EQ(first->acquires, 0u);
}

TEST_P(FrameOutputs, ConsecutiveSurfaceChangesRebuildLatestExtentAfterRetirement)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    output->blockRetire = true;
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    ASSERT_TRUE(Until([&] { return completedCount == 1 && output->presents > 0; }));
    for (uint32_t width : {128u, 144u, 160u}) {
        windows[0]->Resize(width, 80);
        EXPECT_TRUE(std::holds_alternative<OutputProgress>(scheduler->BeginViewport({0, 1})));
        Pump();
        EXPECT_EQ(output->resizes, 0u);
    }
    output->blockRetire = false;
    ASSERT_TRUE(Until([&] { return output->resizes == 1; }));
    const auto prepared = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(prepared));
    EXPECT_EQ(std::get<OutputSnapshot>(prepared).extent.width, 160u);
    EXPECT_EQ(Render({std::get<OutputSnapshot>(prepared)}), 1u);
}

TEST_P(FrameOutputs, VulkanStableFrameQueriesCapabilitiesWithoutEnumeratingOptions)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan surface capability queries";
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    const auto         prepared = scheduler->BeginViewport({0, 1});
    ASSERT_TRUE(std::holds_alternative<OutputSnapshot>(prepared));
    EXPECT_EQ(probe.capsQueries, 1u);
    EXPECT_EQ(Render({std::get<OutputSnapshot>(prepared)}), 1u);
    EXPECT_EQ(probe.capsQueries, 2u);
    EXPECT_EQ(probe.formatQueries, 0u);
    EXPECT_EQ(probe.modeQueries, 0u);
    ASSERT_TRUE(Until([&] { return completedCount == 1; }));
}

TEST_P(FrameOutputs, VulkanSurfaceQueryPreservesErrorsAndRejectsUnsupportedConfiguration)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan surface capability queries";
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    for (const auto error : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_SURFACE_LOST_KHR, VK_ERROR_DEVICE_LOST}) {
        probe.capsResult   = error;
        const auto queried = output->native->QuerySurface();
        EXPECT_EQ(queried.error.nativeCode, error);
        EXPECT_NE(queried.status, PresentationStatus::READY);
        EXPECT_EQ(queried.configuration.extent.width, 0u);
    }
    probe.capsResult = VK_SUCCESS;
    EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
    probe.changeCaps       = [](auto &caps) { caps.supportedUsageFlags &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT; };
    const auto unsupported = output->native->QuerySurface();
    EXPECT_EQ(unsupported.status, PresentationStatus::FAILED);
    EXPECT_EQ(unsupported.error.code, GpuErrorCode::UNSUPPORTED);
    EXPECT_EQ(output->acquires, 0u);
}

TEST_P(FrameOutputs, VulkanLegacyQueriesContinueToEnumerateOptions)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan legacy surface query";
    windows.push_back(std::make_unique<OutputWindow>());
    SwapChain::Descriptor descriptor;
    descriptor.window = windows.back()->handle;
    descriptor.width  = 96;
    descriptor.height = 64;
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    SwapChainPtr       native(device->CreateSwapChain(descriptor));
    ASSERT_TRUE(native);
    EXPECT_EQ(probe.formatQueries, 2u);
    native->Resize(96, 64);
    EXPECT_EQ(probe.formatQueries, 4u);
    for (uint32_t query = 0; query < 2; ++query) {
        EXPECT_EQ(native->QuerySurface().status, PresentationStatus::READY);
        EXPECT_EQ(probe.formatQueries, 6u + 2u * query);
        EXPECT_EQ(probe.modeQueries, 6u + 2u * query);
    }
}

TEST_P(FrameOutputs, VulkanCapabilitiesTrackTransformAndNativeExtent)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan surface capability queries";
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    const auto original = output->native->QuerySurface();
    ASSERT_EQ(original.status, PresentationStatus::READY);
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    probe.changeCaps     = [](auto &caps) { caps.supportedTransforms |= VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR; };
    EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
    probe.changeCaps = [](auto &caps) {
        caps.supportedTransforms |= VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
        caps.currentTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    };
    const auto rotated = output->native->QuerySurface();
    EXPECT_EQ(rotated.status, PresentationStatus::OUT_OF_DATE);
    EXPECT_EQ(rotated.configuration.transform, SurfaceTransform::ROTATE_90);
    EXPECT_EQ(rotated.configuration.extent.width, original.configuration.extent.width);
    EXPECT_EQ(rotated.configuration.revision, original.configuration.revision);
    probe.changeCaps = [](auto &caps) { caps.currentExtent = {UINT32_MAX, UINT32_MAX}; };
    EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
    windows[0]->Resize(128, 80);
    const auto resized = output->native->QuerySurface();
    EXPECT_EQ(resized.status, PresentationStatus::OUT_OF_DATE);
    EXPECT_EQ(resized.configuration.extent.width, 128u);
    EXPECT_EQ(resized.configuration.extent.height, 80u);
}

TEST_P(FrameOutputs, VulkanSuboptimalKeepsImageAndDoesNotRebuildUnchangedConfiguration)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan suboptimal acquisition";
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    probe.acquireSuboptimal = true;
    probe.reverseFormats    = true;
    const auto original     = std::get<OutputSnapshot>(scheduler->BeginViewport({0, 1}));
    for (uint32_t frame = 1; frame <= 3; ++frame) {
        EXPECT_EQ(Render({std::get<OutputSnapshot>(scheduler->BeginViewport({0, 1}))}), 1u);
        ASSERT_TRUE(Until([&] { return completedCount == frame && output->presents >= frame; }));
    }
    EXPECT_EQ(output->resizes, 0u);
    EXPECT_EQ(std::get<OutputSnapshot>(scheduler->BeginViewport({0, 1})).revision, original.revision);
    EXPECT_EQ(probe.formatQueries, 6u);
    EXPECT_EQ(probe.modeQueries, 6u);
}

TEST_P(FrameOutputs, VulkanSuboptimalPresentInvalidatesOptionsOnce)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan presentation options";
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    probe.presentSuboptimal = true;
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    ASSERT_TRUE(Until([&] { return completedCount == 1 && output->presents == 1; }));
    EXPECT_EQ(probe.formatQueries, 0u);
    EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
    EXPECT_EQ(probe.formatQueries, 2u);
    EXPECT_EQ(probe.modeQueries, 2u);
    EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
    EXPECT_EQ(probe.formatQueries, 2u);
    EXPECT_EQ(output->resizes, 0u);
}

TEST_P(FrameOutputs, VulkanChangedOptionsStayInvalidUntilRestoredAndAcquisitionCanRetire)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan presentation options";
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    auto              *output = Add({0, 1});
    ASSERT_TRUE(output);
    probe.acquireSuboptimal = true;
    const auto original     = output->native->QuerySurface();
    for (uint32_t option = 0; option < 3; ++option) {
        SwapChainAcquireResult acquired;
        ASSERT_TRUE(Until([&] {
            acquired = output->native->TryAcquireFrame(0);
            return acquired.status != PresentationStatus::BUSY;
        }));
        ASSERT_EQ(acquired.status, PresentationStatus::READY);
        ASSERT_NE(acquired.frame.serial, 0u);
        if (option < 2) {
            probe.changeFormats = [option, format = original.configuration.format](auto &count, auto *formats) {
                count = 1;
                if (option == 0)
                    formats[0] = {format == PixelFormat::RGBA8_UNORM ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
                else
                    formats[0] = {format == PixelFormat::BGRA8_UNORM ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT};
            };
        } else {
            probe.changeModes = [](auto &count, auto *modes) {
                count    = 1;
                modes[0] = VK_PRESENT_MODE_FIFO_KHR;
            };
        }
        for (uint32_t query = 0; query < 2; ++query) {
            const auto changed = output->native->QuerySurface();
            const auto expected =
                option == 2 && probe.createdPresentMode == VK_PRESENT_MODE_FIFO_KHR ? PresentationStatus::READY : PresentationStatus::OUT_OF_DATE;
            EXPECT_EQ(changed.status, expected);
            EXPECT_EQ(changed.configuration.revision, original.configuration.revision);
        }
        EXPECT_EQ(output->native->TryResize(96, 64).status, PresentationStatus::BUSY);
        probe.changeFormats = {};
        probe.changeModes   = {};
        EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
        const auto queries = probe.formatQueries;
        EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
        EXPECT_EQ(probe.formatQueries, queries);
        ASSERT_TRUE(Until([&] { return output->native->RetireFrame(acquired.frame.serial, true).status == FenceStatus::SIGNALED; }));
    }
}

TEST_P(FrameOutputs, VulkanInvalidatedNegotiationRetriesErrorsAndIncomplete)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan presentation options";
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    probe.acquireSuboptimal = true;
    SwapChainAcquireResult acquired;
    ASSERT_TRUE(Until([&] {
        acquired = output->native->TryAcquireFrame(0);
        return acquired.status != PresentationStatus::BUSY;
    }));
    ASSERT_EQ(acquired.status, PresentationStatus::READY);
    for (auto *result : {&probe.formatResult, &probe.modeResult}) {
        for (const auto error : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_SURFACE_LOST_KHR, VK_ERROR_DEVICE_LOST}) {
            *result            = error;
            const auto queried = output->native->QuerySurface();
            EXPECT_NE(queried.status, PresentationStatus::READY);
            EXPECT_EQ(queried.error.nativeCode, error);
            EXPECT_EQ(queried.configuration.extent.width, 0u);
        }
        *result = VK_SUCCESS;
    }
    for (auto *incomplete : {&probe.formatIncomplete, &probe.modeIncomplete}) {
        *incomplete = true;
        EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::BUSY);
        *incomplete = false;
    }
    EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
    const auto queries = probe.formatQueries;
    EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::READY);
    EXPECT_EQ(probe.formatQueries, queries);
    ASSERT_TRUE(Until([&] { return output->native->RetireFrame(acquired.frame.serial, true).status == FenceStatus::SIGNALED; }));
}

TEST_P(FrameOutputs, VulkanOutOfDateRecreatesWithFullNegotiationThenReturnsToCheapQueries)
{
    if (GetParam() == 2)
        GTEST_SKIP() << "Vulkan presentation options";
    VulkanSurfaceProbe probe(*static_cast<VulkanDevice *>(device));
    auto              *output = Add({0, 1});
    ASSERT_TRUE(output);
    EXPECT_EQ(probe.formatQueries, 2u);
    EXPECT_EQ(probe.modeQueries, 2u);
    const auto original    = output->native->QuerySurface();
    probe.acquireOutOfDate = true;
    const auto acquired    = output->native->TryAcquireFrame(0);
    EXPECT_EQ(acquired.status, PresentationStatus::OUT_OF_DATE);
    EXPECT_EQ(acquired.frame.serial, 0u);
    EXPECT_EQ(output->native->QuerySurface().status, PresentationStatus::OUT_OF_DATE);
    EXPECT_EQ(probe.formatQueries, 4u);
    probe.acquireOutOfDate = false;
    EXPECT_EQ(output->native->TryResize(96, 64).status, PresentationStatus::READY);
    EXPECT_EQ(probe.formatQueries, 6u);
    EXPECT_EQ(probe.modeQueries, 6u);
    const auto recreated = output->native->QuerySurface();
    EXPECT_EQ(recreated.status, PresentationStatus::READY);
    EXPECT_GT(recreated.configuration.revision, original.configuration.revision);
    EXPECT_EQ(probe.formatQueries, 6u);
}

TEST_P(FrameOutputs, DelayedAcquireAndTwoOutputClose)
{
    auto *first  = Add({0, 1});
    auto *second = Add({1, 1});
    ASSERT_TRUE(first && second);
    const auto snapshots = scheduler->Snapshot();
    EXPECT_EQ(first->acquires + second->acquires, 0u);
    EXPECT_EQ(Render(snapshots), 2u);
    scheduler->RequestClose({0, 1});
    EXPECT_TRUE(Until([&] { return closed.size() == 1; }));
    EXPECT_EQ(first->closes, 1u);
    EXPECT_EQ(second->closes, 0u);
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    EXPECT_TRUE(Until([&] { return completedCount == 2; }));
}

TEST_P(FrameOutputs, BusyOutputDoesNotBlockOtherOutputOrControl)
{
    auto *first  = Add({0, 1});
    auto *second = Add({1, 1});
    ASSERT_TRUE(first && second);
    first->blockAcquire = true;
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    scheduler->RequestClose({0, 1});
    EXPECT_TRUE(Until([&] { return closed.size() == 1 && completedCount == 1; }));
    EXPECT_EQ(first->presents, 0u);
    EXPECT_GT(second->presents, 0u);
}

TEST_P(FrameOutputs, ResizeAfterPrepareAndMinimizedOutputAreSkipped)
{
    ASSERT_TRUE(Add({0, 1}));
    ASSERT_TRUE(Add({1, 1}));
    const auto prepared = scheduler->Snapshot();
    windows[0]->Resize(128, 80);
    scheduler->RequestResize({0, 1}, {128, 80});
    EXPECT_EQ(Render(prepared), 1u);
    EXPECT_TRUE(Until([&] { return outputs[0]->resizes == 1 && completedCount == 1; }));
    scheduler->RequestResize({1, 1}, {0, 0});
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    EXPECT_TRUE(Until([&] { return completedCount == 2; }));
    scheduler->RequestResize({1, 1}, {96, 64});
    EXPECT_TRUE(Until([&] { return Render(scheduler->Snapshot()) == 2; }));
}

TEST_P(FrameOutputs, CompileFailureReleasesAcquireAfterCpuBorrowersExit)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    const auto lease    = Begin();
    auto       acquired = scheduler->Acquire(lease, scheduler->Snapshot());
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto graph = Graph(std::move(std::get<OutputAcquireBatch>(acquired)));
    ASSERT_TRUE(graph);
    auto data = graph->GetData();
    data.passes.emplace_back(FrameComputePass{});
    auto invalid = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, invalid)));
    scheduler->Abandon(lease);
    scheduler->RequestClose({0, 1});
    Pump();
    EXPECT_EQ(output->presents, 0u);
    EXPECT_EQ(output->closes, 0u);
    invalid.reset();
    graph.reset();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(Until([&] { return scheduler->Empty(); }));
    EXPECT_EQ(output->presents, 0u);
}

TEST_P(FrameOutputs, GraphicsCompletionDoesNotAcknowledgePresentationRetirement)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    output->blockRetire = true;
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    scheduler->RequestClose({0, 1});
    EXPECT_TRUE(Until([&] { return completedCount == 1 && output->retires > 0; }));
    EXPECT_TRUE(closed.empty());
    EXPECT_EQ(output->closes, 0u);
    output->blockRetire = false;
    EXPECT_TRUE(Until([&] { return scheduler->Empty(); }));
}

TEST_P(FrameOutputs, PresentRetryKeepsOutputAliveAndOtherOutputProgresses)
{
    auto *first  = Add({0, 1});
    auto *second = Add({1, 1});
    ASSERT_TRUE(first && second);
    first->blockPresent = true;
    EXPECT_EQ(Render(scheduler->Snapshot()), 2u);
    scheduler->RequestClose({0, 1});
    EXPECT_TRUE(Until([&] { return completedCount == 1 && second->retires > 0; }));
    EXPECT_TRUE(closed.empty());
    EXPECT_GT(first->presents, 0u);
    first->blockPresent = false;
    EXPECT_TRUE(Until([&] { return closed.size() == 1; }));
}

TEST_P(FrameOutputs, EmptyOutputFrameDoesNotSubmitOrResetSlot)
{
    EXPECT_EQ(Render({}), 0u);
    EXPECT_EQ(completedCount, 0u);
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    EXPECT_TRUE(Until([&] { return completedCount == 1; }));
}

TEST_P(FrameOutputs, AllOutputsBusyAndStaleSlotDoNotAcquireOrSubmit)
{
    auto *first  = Add({0, 1});
    auto *second = Add({1, 1});
    ASSERT_TRUE(first && second);
    first->blockAcquire = second->blockAcquire = true;
    EXPECT_EQ(Render(scheduler->Snapshot()), 0u);
    const auto lease = Begin();
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    const auto calls = first->acquires + second->acquires;
    EXPECT_TRUE(std::holds_alternative<FrameError>(scheduler->Acquire(lease, scheduler->Snapshot())));
    EXPECT_EQ(first->acquires + second->acquires, calls);
    first->blockAcquire = second->blockAcquire = false;
    EXPECT_EQ(Render(scheduler->Snapshot()), 2u);
    EXPECT_TRUE(Until([&] { return completedCount == 1; }));
}

TEST_P(FrameOutputs, MissingExportRejectsSubmissionAndReleasesBothAcquires)
{
    ASSERT_TRUE(Add({0, 1}));
    ASSERT_TRUE(Add({1, 1}));
    const auto lease    = Begin();
    auto       acquired = scheduler->Acquire(lease, scheduler->Snapshot());
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto batch = std::move(std::get<OutputAcquireBatch>(acquired));
    ASSERT_EQ(batch.outputs.size(), 2u);
    batch.outputs.pop_back();
    auto recorded = Record(lease, Graph(std::move(batch)));
    ASSERT_TRUE(recorded);
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(scheduler->Submit(recorded)));
    EXPECT_TRUE(recorded);
    recorded.reset();
    scheduler->Abandon(lease);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    scheduler->RequestShutdown();
    EXPECT_TRUE(Until([&] { return scheduler->Empty(); }));
    EXPECT_EQ(outputs[0]->presents + outputs[1]->presents, 0u);
    EXPECT_EQ(completedCount, 0u);
}

TEST_P(FrameOutputs, BorrowedOutputCannotBypassSchedulerSynchronization)
{
    ASSERT_TRUE(Add({0, 1}));
    const auto lease    = Begin();
    auto       acquired = scheduler->Acquire(lease, scheduler->Snapshot());
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto recorded = Record(lease, Graph(std::move(std::get<OutputAcquireBatch>(acquired))));
    ASSERT_TRUE(recorded);
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(context->Submit(recorded)));
    ASSERT_TRUE(recorded);
    EXPECT_TRUE(std::holds_alternative<SubmissionAccepted>(scheduler->Submit(recorded)));
    EXPECT_TRUE(Until([&] { return completedCount == 1; }));
}

TEST_P(FrameOutputs, NativeAcquireFailurePreservesErrorAndOtherOutputProgresses)
{
    ASSERT_TRUE(Add({0, 1}));
    ASSERT_TRUE(Add({1, 1}));
    const auto                                   snapshots = scheduler->Snapshot();
    const auto                                   lease     = Begin();
    std::variant<FrameError, OutputAcquireBatch> acquired;
    int64_t                                      expected = 0;
    if (GetParam() == 2) {
        auto      &functions = const_cast<D3D12Functions &>(static_cast<D3D12Device *>(device)->GetFunctions());
        const auto saved     = functions;
        functions.removed    = [](ID3D12Device *) -> HRESULT { return DXGI_ERROR_DEVICE_REMOVED; };
        acquired             = scheduler->Acquire(lease, std::span(snapshots).first(1));
        functions            = saved;
        expected             = DXGI_ERROR_DEVICE_REMOVED;
    } else {
        auto      &functions            = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
        const auto saved                = functions;
        functions.vkAcquireNextImageKHR = [](VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t *) -> VkResult {
            return VK_ERROR_SURFACE_LOST_KHR;
        };
        acquired  = scheduler->Acquire(lease, std::span(snapshots).first(1));
        functions = saved;
        expected  = VK_ERROR_SURFACE_LOST_KHR;
    }
    if (GetParam() == 2) {
        ASSERT_TRUE(std::holds_alternative<FrameError>(acquired));
        EXPECT_EQ(std::get<FrameError>(acquired).code, FrameErrorCode::DEVICE_LOST);
        EXPECT_EQ(std::get<FrameError>(acquired).cause.nativeCode, expected);
        EXPECT_TRUE(std::holds_alternative<FrameError>(scheduler->Acquire(lease, std::span(snapshots).last(1))));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        return;
    }
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    const auto &failed = std::get<OutputAcquireBatch>(acquired);
    ASSERT_EQ(failed.skipped.size(), 1u);
    EXPECT_EQ(failed.skipped[0].error.nativeCode, expected);
    EXPECT_TRUE(failed.outputs.empty());
    auto other = scheduler->Acquire(lease, std::span(snapshots).last(1));
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(other));
    auto recorded = Record(lease, Graph(std::move(std::get<OutputAcquireBatch>(other))));
    ASSERT_TRUE(recorded);
    EXPECT_TRUE(std::holds_alternative<SubmissionAccepted>(scheduler->Submit(recorded)));
    scheduler->RequestClose({0, 1});
    EXPECT_TRUE(Until([&] { return closed.size() == 1 && completedCount == 1; }));
}

TEST_P(FrameOutputs, NativePresentRetryDoesNotConsumeTheAcquiredFrame)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    output->blockPresent = true;
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    EXPECT_TRUE(Until([&] { return completedCount == 1; }));
    output->blockPresent = false;
    if (GetParam() == 2) {
        auto      &functions = const_cast<D3D12Functions &>(static_cast<D3D12Device *>(device)->GetFunctions());
        const auto saved     = functions;
        functions.present    = [](IDXGISwapChain *, UINT, UINT flags) -> HRESULT {
            EXPECT_NE(flags & DXGI_PRESENT_DO_NOT_WAIT, 0u);
            return DXGI_ERROR_WAS_STILL_DRAWING;
        };
        Pump();
        functions = saved;
    } else {
        auto      &functions        = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
        const auto saved            = functions;
        functions.vkQueuePresentKHR = [](VkQueue, const VkPresentInfoKHR *) -> VkResult { return VK_ERROR_OUT_OF_HOST_MEMORY; };
        Pump();
        functions = saved;
    }
    EXPECT_EQ(output->retires, 0u);
    scheduler->RequestClose({0, 1});
    EXPECT_TRUE(Until([&] { return scheduler->Empty(); }));
}

TEST_P(FrameOutputs, SurfaceChangedDuringAcquireAbandonsOnlyThatOutput)
{
    auto *first = Add({0, 1});
    ASSERT_TRUE(first);
    ASSERT_TRUE(Add({1, 1}));
    first->afterAcquire = [&] { windows[0]->Resize(128, 80); };
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    EXPECT_TRUE(Until([&] { return completedCount == 1 && first->resizes == 1; }));
    EXPECT_EQ(first->presents, 0u);
    EXPECT_EQ(scheduler->Snapshot()[0].extent.width, 128u);
    EXPECT_EQ(Render(scheduler->Snapshot()), 2u);
}

TEST_P(FrameOutputs, NativeResizeFailurePreservesErrorAndStillCloses)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    PresentationResult resized;
    int64_t            expected = 0;
    if (GetParam() == 2) {
        auto      &functions    = const_cast<D3D12Functions &>(static_cast<D3D12Device *>(device)->GetFunctions());
        const auto saved        = functions;
        functions.resizeBuffers = [](IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT) -> HRESULT { return E_OUTOFMEMORY; };
        resized                 = output->native->TryResize(96, 64);
        functions               = saved;
        expected                = E_OUTOFMEMORY;
    } else {
        auto      &functions           = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
        const auto saved               = functions;
        functions.vkCreateSwapchainKHR = [](VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *) -> VkResult {
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;
        };
        resized   = output->native->TryResize(96, 64);
        functions = saved;
        expected  = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    EXPECT_EQ(resized.status, PresentationStatus::FAILED);
    EXPECT_EQ(resized.error.code, GpuErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(resized.error.nativeCode, expected);
    scheduler->RequestClose({0, 1});
    EXPECT_TRUE(Until([&] { return scheduler->Empty(); }));
}

TEST_P(FrameOutputs, NoOutputUploadStillUsesTheSameFrameContext)
{
    const auto lease    = Begin();
    auto       acquired = scheduler->Acquire(lease, {});
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    Buffer::Descriptor sourceInfo;
    sourceInfo.size   = 16;
    sourceInfo.usage  = BufferUsageFlagBit::TRANSFER_SRC;
    sourceInfo.memory = MemoryType::CPU_TO_GPU;
    BufferPtr source(device->CreateBuffer(sourceInfo));
    auto      targetInfo = sourceInfo;
    targetInfo.usage     = BufferUsageFlagBit::TRANSFER_DST;
    targetInfo.memory    = MemoryType::GPU_TO_CPU;
    BufferPtr target(device->CreateBuffer(targetInfo));
    ASSERT_TRUE(source && target);
    const std::array<uint32_t, 4> expected{11, 22, 33, 44};
    std::memcpy(source->Map(), expected.data(), sizeof(expected));
    source->UnMap();
    FrameGraphBuilder builder;
    const auto src = std::get<GraphBufferHandle>(builder.Import({source, 16, CopyBufferState::HOST_WRITE, CopyBufferState::COPY_SOURCE, true}));
    const auto dst = std::get<GraphBufferHandle>(builder.Import({target, 16, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ}));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({src, dst, 0, 0, 16})));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({dst, 0, 16})));
    auto frozen = builder.Freeze();
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrozenFrameGraph>>(frozen));
    auto recorded = Record(lease, std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen));
    ASSERT_TRUE(recorded);
    EXPECT_TRUE(std::holds_alternative<SubmissionAccepted>(scheduler->Submit(recorded)));
    ASSERT_TRUE(Until([&] { return completedCount == 1; }));
    EXPECT_EQ(std::memcmp(target->Map(), expected.data(), sizeof(expected)), 0);
    target->UnMap();
}

TEST_P(FrameOutputs, AcquireFailurePreservesOtherOutputOffscreenAndCopySubmission)
{
    VerifySkippedOutputSubmission(false);
}

TEST_P(FrameOutputs, AllAcquiresFailButOffscreenAndCopyStillSubmit)
{
    VerifySkippedOutputSubmission(true);
}

TEST_P(FrameOutputs, UncertainSubmissionNeverPresentsOrAcknowledgesClose)
{
    auto *output = Add({0, 1});
    ASSERT_TRUE(output);
    const auto lease    = Begin();
    auto       acquired = scheduler->Acquire(lease, scheduler->Snapshot());
    ASSERT_TRUE(std::holds_alternative<OutputAcquireBatch>(acquired));
    auto recorded = Record(lease, Graph(std::move(std::get<OutputAcquireBatch>(acquired))));
    ASSERT_TRUE(recorded);
    FrameSubmitResult submitted;
    if (GetParam() == 2) {
        auto      &functions = const_cast<D3D12Functions &>(static_cast<D3D12Device *>(device)->GetFunctions());
        const auto saved     = functions;
        functions.signal     = [](ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value) -> HRESULT {
            queue->Signal(fence, value);
            return E_FAIL;
        };
        submitted = scheduler->Submit(recorded);
        functions = saved;
    } else {
        auto      &functions     = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
        const auto saved         = functions;
        functions.vkQueueSubmit2 = [](VkQueue, uint32_t, const VkSubmitInfo2 *, VkFence) -> VkResult { return VK_ERROR_DEVICE_LOST; };
        submitted                = scheduler->Submit(recorded);
        functions                = saved;
    }
    terminalTeardown = true;
    EXPECT_TRUE(std::holds_alternative<SubmissionIndeterminate>(submitted));
    EXPECT_FALSE(recorded);
    scheduler->RequestClose({0, 1});
    Pump();
    EXPECT_FALSE(scheduler->Empty());
    EXPECT_TRUE(closed.empty());
    EXPECT_EQ(output->presents, 0u);
    EXPECT_EQ(output->closes, 0u);
}

TEST_P(FrameOutputs, SurfaceLostDuringResizeReportsOnceAndOtherOutputContinues)
{
    auto *first = Add({0, 1});
    ASSERT_TRUE(first);
    ASSERT_TRUE(Add({1, 1}));
    first->resizeFailure = PresentationResult{PresentationStatus::SURFACE_LOST, {GpuErrorCode::NATIVE_ERROR, -44}};
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(scheduler->RequestResize({0, 1}, {128, 80})));
    std::array<OutputProgress, 2> progress;
    ASSERT_EQ(scheduler->Pump(progress), 1u);
    EXPECT_EQ(progress[0].kind, OutputProgressKind::FAILED);
    EXPECT_EQ(progress[0].error.nativeCode, -44);
    EXPECT_EQ(scheduler->Pump(progress), 0u);
    EXPECT_EQ(first->resizes, 1u);
    EXPECT_EQ(Render(scheduler->Snapshot()), 1u);
    scheduler->RequestClose({0, 1});
    EXPECT_TRUE(Until([&] { return closed.size() == 1 && completedCount == 1; }));
}

INSTANTIATE_TEST_SUITE_P(NativeCompilers, FrameOutputs, testing::Values(0u, 1u, 2u));
