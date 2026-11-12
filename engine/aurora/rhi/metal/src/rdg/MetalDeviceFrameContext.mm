#include <rdg/MetalDeviceFrameContext.h>

#include "MetalFrameCompiler.h"
#include <Metal/Metal.h>
#include <MetalBuffer.h>
#include <MetalCommandPool.h>
#include <MetalDevice.h>

namespace sky::aurora {
    namespace {
        FrameError MakeError(FrameErrorCode code, const char *message = nullptr)
        {
            return FrameError{code, message ? message : std::string{}};
        }
    } // namespace

    FrameBufferStorageResult MetalDeviceFrameContext::CreateFrameBuffer(const Buffer::Descriptor &desc)
    {
        auto buffer = std::make_unique<MetalBuffer>(*device);
        if (!buffer->Init(desc))
            return MakeError(FrameErrorCode::OUT_OF_MEMORY, "Metal frame buffer allocation failed");
        uint8_t *mapped = nullptr;
        if (desc.memory != MemoryType::GPU_ONLY) {
            mapped = buffer->Map();
            if (!mapped)
                return MakeError(FrameErrorCode::UNSUPPORTED, "Metal frame buffer is not host-visible");
        }
        return FrameBufferStorage{BufferPtr(buffer.release()), mapped};
    }

    FrameCompileResult MetalDeviceFrameContext::Compile(const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph)
    {
        @autoreleasepool {
            auto valid = ValidateCompileLease(lease);
            if (auto *error = std::get_if<FrameError>(&valid))
                return *error;
            if (!graph)
                return MakeError(FrameErrorCode::INVALID_ARGUMENT);
            MetalFrameCompiler compiler(*device);
            auto               result = compiler.Compile(std::move(graph), lease.context.deviceEpoch);
            if (auto *error = std::get_if<FrameError>(&result)) {
                ObserveFailure(*error);
                return *error;
            }
            if (std::holds_alternative<FrameNoWork>(result))
                return FrameNoWork{};
            auto plan     = std::move(std::get<std::unique_ptr<MetalCompiledFrame>>(result));
            auto reserved = ReservePlan(lease, *plan->graph, plan->resources);
            if (auto *error = std::get_if<FrameError>(&reserved))
                return *error;
            plan->reservation = std::move(std::get<FramePlanLease>(reserved));
            issuedPlan        = plan.get();
            issuedIdentity    = plan->GetIdentity();
            return std::unique_ptr<BackendCompiledFrame>(std::move(plan));
        }
    }

    FrameDispatchResult MetalDeviceFrameContext::Dispatch(std::unique_ptr<BackendCompiledFrame> &compiled)
    {
        @autoreleasepool {
            if (!compiled || compiled.get() != issuedPlan || compiled->GetIdentity() != issuedIdentity)
                return MakeError(FrameErrorCode::WRONG_CONTEXT);
            auto *plan = static_cast<MetalCompiledFrame *>(compiled.get());

            auto started = BeginEncoding(plan->reservation);
            if (auto *error = std::get_if<FrameError>(&started))
                return *error;
            const auto ticket = std::get<EncodingTicket>(started);
            std::unique_ptr<MetalCompiledFrame> encoding(static_cast<MetalCompiledFrame *>(compiled.release()));
            issuedPlan = nullptr;

            auto allocation = AllocatePlanCommand(plan->reservation, 0);
            if (auto *error = std::get_if<FrameError>(&allocation)) {
                PublishEncoding(ticket, *error);
                return ticket;
            }
            auto *command = static_cast<MetalCommandBuffer *>(std::get<CommandBuffer *>(allocation));
            command->Begin();
            auto encodeError = plan->encoder->Encode(*device, *command);
            command->End();
            if (const auto error = command->GetRecordingError();
                error.code != GpuErrorCode::NONE && (!encodeError || error.code == GpuErrorCode::DEVICE_LOST))
                encodeError = FrameError{FrameErrorCode::ENCODE_FAILED, "Metal native recording failed", error};
            if (encodeError) {
                PublishEncoding(ticket, std::move(*encodeError));
                return ticket;
            }

            FrameRecordingInputs inputs;
            inputs.receipt = {plan->graph, plan->livePasses};
            inputs.payload = std::make_shared<FrameGpuPayload<MetalFrameEncoder>>(std::move(plan->resources), std::move(plan->encoder));
            PublishEncoding(ticket, FinishPlanRecording(plan->reservation, std::move(inputs)));
            return ticket;
        }
    }

} // namespace sky::aurora
