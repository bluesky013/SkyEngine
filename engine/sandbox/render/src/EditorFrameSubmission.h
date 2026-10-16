#pragma once

#include <aurora/rdg/RenderDeviceExclusive.h>
#include <aurora/rdg/RenderViewport.h>
#include <aurora/rhi/Queue.h>
#include <aurora/rhi/SubmitInfo.h>

namespace sky::editor {

    // Every BeginFrame resets the current fence, including frames with no work.
    // Scope completion always submits that fence and balances each successful
    // viewport acquire before advancing the frame context.
    class EditorFrameSubmission final {
    public:
        EditorFrameSubmission(sky::aurora::DeviceFrameContext &frameContext, sky::aurora::Queue &queue) : frameContext(frameContext), queue(queue)
        {
            frameContext.BeginFrame();
        }

        ~EditorFrameSubmission()
        {
            submit.fence = frameContext.GetFrameFence();
            queue.Submit(submit);
            for (auto *viewport : acquired) {
                viewport->Release();
            }
            frameContext.EndFrame();
        }

        EditorFrameSubmission(const EditorFrameSubmission &)            = delete;
        EditorFrameSubmission &operator=(const EditorFrameSubmission &) = delete;

        bool Acquire(sky::aurora::RenderViewport &viewport)
        {
            if (!viewport.Begin()) {
                return false;
            }
            const auto extent = viewport.GetExtent();
            if (extent.width == 0 || extent.height == 0 || !viewport.Acquire()) {
                return false;
            }

            acquired.push_back(&viewport);
            if (auto *semaphore = viewport.GetAcquireSemaphore()) {
                // Wait before any work, including a preview's transfer writes.
                submit.waitSemaphores.push_back({semaphore, 0, sky::aurora::PipelineStageBit::TOP});
            }
            if (auto *semaphore = viewport.GetRenderDoneSemaphore()) {
                submit.signalSemaphores.push_back({semaphore, 0, sky::aurora::PipelineStageBit::BOTTOM});
            }
            // Keep acquired-but-unusable images in the submission: presenting
            // them still requires consuming acquire and signaling render-done.
            return viewport.GetBackbuffer() != nullptr;
        }

        void AddCommandBuffer(sky::aurora::CommandBuffer *commandBuffer)
        {
            submit.commandBuffers.push_back(commandBuffer);
        }

    private:
        sky::aurora::DeviceFrameContext           &frameContext;
        sky::aurora::Queue                        &queue;
        sky::aurora::SubmitInfo                    submit;
        std::vector<sky::aurora::RenderViewport *> acquired;
    };

} // namespace sky::editor
