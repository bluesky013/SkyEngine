#include "EditorFrameSubmission.h"

#include <aurora/rhi/Fence.h>
#include <aurora/rhi/Semaphore.h>
#include <gtest/gtest.h>

namespace sky::editor {
    namespace {
        using namespace sky::aurora;

        class TestFence final : public Fence {
        public:
            TestFence()
            {
                counter.store(0);
            }
            bool     signaled         = true;
            uint32_t uncompletedWaits = 0;

            void Wait() override
            {
                if (!signaled) {
                    ++uncompletedWaits;
                }
            }
            void Reset() override
            {
                signaled = false;
            }
            bool IsSignaled() override
            {
                return signaled;
            }
            bool WaitFor(uint64_t) override
            {
                return signaled;
            }
        };

        class TestFrameContext final : public DeviceFrameContext {
        public:
            TestFrameContext() : fence(new TestFence) {}

            FrameContextIdentity GetIdentity() const noexcept override
            {
                return {};
            }
            BeginSlotResult TryBeginSlot() noexcept override
            {
                return FrameError{FrameErrorCode::UNSUPPORTED};
            }
            std::variant<FrameError, uint64_t> AvailableFrameBufferBytes(const DeviceFrameLease &, FrameBufferKind, uint64_t) const override
            {
                return FrameError{FrameErrorCode::UNSUPPORTED};
            }
            FrameBufferAllocation WriteFrameBuffer(const DeviceFrameLease &, FrameBufferKind, std::span<const std::byte>, uint64_t) override
            {
                return FrameError{FrameErrorCode::UNSUPPORTED};
            }
            FrameCompileResult Compile(const DeviceFrameLease &, std::shared_ptr<const FrozenFrameGraph>) override
            {
                return FrameError{FrameErrorCode::UNSUPPORTED};
            }
            FrameDispatchResult Dispatch(std::unique_ptr<BackendCompiledFrame> &) override
            {
                return FrameError{FrameErrorCode::UNSUPPORTED};
            }
            FrameCollectResult TryCollect(const EncodingTicket &) override
            {
                return FrameError{FrameErrorCode::UNSUPPORTED};
            }
            FrameOperationResult Cancel(const EncodingTicket &) override
            {
                return FrameError{FrameErrorCode::UNSUPPORTED};
            }
            FrameSubmitResult Submit(std::unique_ptr<RecordedFrame> &) noexcept override
            {
                return SubmissionRejected{FrameError{FrameErrorCode::UNSUPPORTED}};
            }
            FrameOperationResult DiscardUnsubmitted(const DeviceFrameLease &) noexcept override
            {
                return FrameError{FrameErrorCode::UNSUPPORTED};
            }
            size_t PollCompleted(std::span<FrameCompletion>) noexcept override
            {
                return 0;
            }
            void BeginFrame() noexcept override
            {
                ++begins;
                fence->Reset();
            }
            void EndFrame() noexcept override
            {
                ++ends;
            }
            Fence *GetFrameFence() const noexcept override
            {
                return fence.Get();
            }

            CounterPtr<TestFence> fence;
            uint32_t              begins = 0;
            uint32_t              ends   = 0;
        };

        class TestSemaphore final : public sky::aurora::Semaphore {
        public:
            bool          signaled = false;
            SemaphoreType GetType() const override
            {
                return SemaphoreType::BINARY;
            }
            void Signal(uint64_t) override
            {
                signaled = true;
            }
            bool Wait(uint64_t, uint64_t) override
            {
                return signaled;
            }
            uint64_t GetCurrentValue() const override
            {
                return signaled ? 1 : 0;
            }
        };

        class TestQueue final : public Queue {
        public:
            std::vector<SubmitInfo> submissions;

            void Submit(const SubmitInfo &info) override
            {
                submissions.push_back(info);
                for (const auto &wait : info.waitSemaphores) {
                    auto *semaphore = static_cast<TestSemaphore *>(wait.semaphore);
                    EXPECT_TRUE(semaphore->signaled);
                    semaphore->signaled = false;
                }
                for (const auto &signal : info.signalSemaphores) {
                    static_cast<TestSemaphore *>(signal.semaphore)->signaled = true;
                }
                if (info.fence != nullptr) {
                    static_cast<TestFence *>(info.fence)->signaled = true;
                }
            }
            void WaitIdle() override
            {
            }
            QueueType GetType() const override
            {
                return QueueType::GRAPHICS;
            }
            TransferTaskHandle UploadBuffer(Buffer *, const std::vector<BufferUploadRequest> &) override
            {
                return 0;
            }
            TransferTaskHandle UploadImage(Image *, const std::vector<ImageUploadRequest> &) override
            {
                return 0;
            }
            void Wait(TransferTaskHandle) override
            {
            }
            bool HasComplete(TransferTaskHandle) const override
            {
                return true;
            }
        };

        class TestViewport final : public RenderViewport {
        public:
            bool          beginSucceeds   = true;
            bool          acquireSucceeds = true;
            bool          hasBackbuffer   = true;
            Extent2D      extent{64, 64};
            TestSemaphore acquireSemaphore;
            TestSemaphore renderDoneSemaphore;
            uint32_t      acquireCalls = 0;
            uint32_t      releases     = 0;

            bool Begin() override
            {
                return beginSucceeds;
            }
            bool Acquire() override
            {
                ++acquireCalls;
                if (acquireSucceeds) {
                    EXPECT_FALSE(acquireSemaphore.signaled);
                    acquireSemaphore.signaled = true;
                }
                return acquireSucceeds;
            }
            void Release() override
            {
                EXPECT_FALSE(acquireSemaphore.signaled);
                EXPECT_TRUE(renderDoneSemaphore.signaled);
                renderDoneSemaphore.signaled = false;
                ++releases;
            }
            Image *GetBackbuffer() const override
            {
                // Opaque identity only: the frame coordinator never dereferences it.
                return hasBackbuffer ? reinterpret_cast<Image *>(const_cast<TestViewport *>(this)) : nullptr;
            }
            PixelFormat GetFormat() const override
            {
                return PixelFormat::BGRA8_UNORM;
            }
            Extent2D GetExtent() const override
            {
                return extent;
            }
            const Name &GetName() const override
            {
                return name;
            }
            sky::aurora::Semaphore *GetAcquireSemaphore() const override
            {
                return const_cast<TestSemaphore *>(&acquireSemaphore);
            }
            sky::aurora::Semaphore *GetRenderDoneSemaphore() const override
            {
                return const_cast<TestSemaphore *>(&renderDoneSemaphore);
            }

        private:
            Name name;
        };
    } // namespace

    TEST(EditorFrameSubmissionTest, SkippedFramesSignalResetFenceAndNextFrameCanBegin)
    {
        TestFrameContext context;
        TestQueue        queue;
        TestViewport     viewport;

        for (uint32_t failure = 0; failure < 3; ++failure) {
            viewport.beginSucceeds   = failure != 0;
            viewport.extent          = failure == 1 ? Extent2D{0, 64} : Extent2D{64, 64};
            viewport.acquireSucceeds = failure != 2;
            {
                EditorFrameSubmission frame(context, queue);
                EXPECT_FALSE(context.fence->IsSignaled());
                EXPECT_FALSE(frame.Acquire(viewport));
            }
            ASSERT_EQ(queue.submissions.size(), failure + 1);
            EXPECT_TRUE(queue.submissions.back().commandBuffers.empty());
            EXPECT_TRUE(queue.submissions.back().waitSemaphores.empty());
            EXPECT_TRUE(queue.submissions.back().signalSemaphores.empty());
            EXPECT_EQ(queue.submissions.back().fence, context.fence.Get());
            EXPECT_TRUE(context.fence->IsSignaled());
        }
        EXPECT_EQ(viewport.acquireCalls, 1u);
        EXPECT_EQ(viewport.releases, 0u);

        viewport.acquireSucceeds = true;
        {
            EditorFrameSubmission recovered(context, queue);
            EXPECT_TRUE(recovered.Acquire(viewport));
        }
        EXPECT_EQ(context.fence->uncompletedWaits, 0u);
        EXPECT_EQ(context.begins, 4u);
        EXPECT_EQ(context.ends, 4u);
        EXPECT_EQ(viewport.releases, 1u);
    }

    TEST(EditorFrameSubmissionTest, AcquiredImageWithoutBackbufferBalancesSemaphoresBeforeRelease)
    {
        TestFrameContext context;
        TestQueue        queue;
        TestViewport     viewport;
        viewport.hasBackbuffer = false;
        {
            EditorFrameSubmission frame(context, queue);
            EXPECT_FALSE(frame.Acquire(viewport));
            EXPECT_EQ(viewport.releases, 0u);
        }
        ASSERT_EQ(queue.submissions.size(), 1u);
        EXPECT_TRUE(queue.submissions.front().commandBuffers.empty());
        ASSERT_EQ(queue.submissions.front().waitSemaphores.size(), 1u);
        EXPECT_EQ(queue.submissions.front().waitSemaphores.front().semaphore, &viewport.acquireSemaphore);
        ASSERT_EQ(queue.submissions.front().signalSemaphores.size(), 1u);
        EXPECT_EQ(queue.submissions.front().signalSemaphores.front().semaphore, &viewport.renderDoneSemaphore);
        EXPECT_EQ(viewport.releases, 1u);

        viewport.hasBackbuffer = true;
        {
            EditorFrameSubmission recovered(context, queue);
            EXPECT_TRUE(recovered.Acquire(viewport));
        }
        EXPECT_EQ(context.fence->uncompletedWaits, 0u);
        EXPECT_EQ(viewport.releases, 2u);
    }

    TEST(EditorFrameSubmissionTest, MultipleAcquiredWindowsShareOneSubmissionIncludingUnusableWindow)
    {
        TestFrameContext context;
        TestQueue        queue;
        TestViewport     main, preview, floating;
        floating.hasBackbuffer = false;
        // Command-buffer identities are passed through without dereferencing.
        uint32_t mainToken = 0, previewToken = 0;
        auto    *mainCommands    = reinterpret_cast<CommandBuffer *>(&mainToken);
        auto    *previewCommands = reinterpret_cast<CommandBuffer *>(&previewToken);
        {
            EditorFrameSubmission frame(context, queue);
            EXPECT_TRUE(frame.Acquire(main));
            frame.AddCommandBuffer(mainCommands);
            EXPECT_TRUE(frame.Acquire(preview));
            frame.AddCommandBuffer(previewCommands);
            EXPECT_FALSE(frame.Acquire(floating));
        }
        ASSERT_EQ(queue.submissions.size(), 1u);
        const auto &submit = queue.submissions.front();
        ASSERT_EQ(submit.commandBuffers.size(), 2u);
        EXPECT_EQ(submit.commandBuffers[0], mainCommands);
        EXPECT_EQ(submit.commandBuffers[1], previewCommands);
        EXPECT_EQ(submit.waitSemaphores.size(), 3u);
        EXPECT_EQ(submit.signalSemaphores.size(), 3u);
        EXPECT_EQ(submit.fence, context.fence.Get());
        EXPECT_EQ(main.releases, 1u);
        EXPECT_EQ(preview.releases, 1u);
        EXPECT_EQ(floating.releases, 1u);
        EXPECT_EQ(context.begins, 1u);
        EXPECT_EQ(context.ends, 1u);
    }
} // namespace sky::editor
