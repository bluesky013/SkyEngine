#pragma once

#include "frame/FramePacketQueue.h"
#include "prepare/ScenePrepareScheduler.h"
#include "prepare/SerialPrepareExecutor.h"
#include "scene/FrozenCpuFrame.h"

namespace sky::aurora {

    using CpuPrepareBuildResult = std::variant<RendererError, CpuPrepareProgram>;
    using CpuPrepareBuilder     = std::function<CpuPrepareBuildResult(const FrozenCpuFrame &)>;

    struct CpuPacketCompleted {
        PacketApplyResult            application;
        PrepareResult                preparation;
        bool                         preparationAttempted = false;
        std::optional<RendererError> executionError;
        bool                         Succeeded() const
        {
            return application.Succeeded() && preparationAttempted && preparation.Succeeded() && !executionError;
        }
    };
    struct CpuCoordinatorStopped {
        std::optional<RendererError> error;
        size_t                       discardedDeferredCommands = 0;
    };
    using CpuCoordinatorEvent = std::variant<CpuPacketCompleted, CpuCoordinatorStopped>;
    enum class CpuCoordinatorState : uint8_t { RUNNING, STOPPING, STOPPED };

    struct CpuCoordinatorInit {
        uint32_t         maxPendingPackets     = 2;
        uint32_t         maxOutstandingResults = 3;
        ScenePrepareInit prepare;
    };

    // Owned and called only by the coordinator thread, including teardown after all CPU borrowers drain.
    class CpuCoordinatorRuntime {
    public:
        virtual ~CpuCoordinatorRuntime()                                                                        = default;
        virtual std::optional<RendererError> Initialize()                                                       = 0;
        virtual std::optional<RendererError> Validate(const FramePacket &) const                                = 0;
        virtual std::optional<RendererError> Applied(CpuSceneStore &, FramePacket &, const PacketApplyResult &) = 0;
        virtual CpuPrepareBuildResult        Build(const FrozenCpuFrame &)                                      = 0;
        virtual bool                         RecoverPrepareFailure(const CpuPacketCompleted &)                  = 0;
        virtual std::optional<RendererError> Prepared(CpuSceneStore &, const CpuPacketCompleted &)              = 0;
        virtual std::optional<RendererError> Pump(CpuSceneStore &)                                              = 0;
        virtual std::optional<RendererError> Shutdown(CpuSceneStore &)                                          = 0;
    };

    // CPU stage only, not IRenderer: retirement records still require the later GPU/presentation protocol.
    // The host thread owns submission, polling, shutdown and Join.
    class CpuRenderCoordinator {
    public:
        CpuRenderCoordinator();
        std::optional<RendererError>
        Init(const CpuCoordinatorInit &info, CpuPrepareBuilder builder = {}, std::unique_ptr<CpuCoordinatorRuntime> runtime = {});
        ~CpuRenderCoordinator();
        CpuRenderCoordinator(const CpuRenderCoordinator &)            = delete;
        CpuRenderCoordinator &operator=(const CpuRenderCoordinator &) = delete;

        PacketSubmitResult  TrySubmit(std::unique_ptr<FramePacket> &packet);
        size_t              PollEvents(std::span<CpuCoordinatorEvent> destination);
        CpuCoordinatorState GetState() const;
        void                RequestShutdown();
        // Cancels only the current prepare; failure/cancellation requires rebuilding the coordinator's scene state.
        bool RequestCancelPrepare();
        // Request shutdown and release borrowed views before Join; shutdown requests never invalidate active readers.
        void Join();

    private:
        struct State;
        std::unique_ptr<State> state;
    };

} // namespace sky::aurora
