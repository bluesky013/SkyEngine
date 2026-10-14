#pragma once

#include <aurora/rdg/FrameExecutionTypes.h>

namespace sky::aurora {

    class DeviceFrameContext;
    class FrozenFrameGraph;

    struct FrameCompileContext {
        DeviceFrameContext &frameContext;
        DeviceFrameLease    lease;
    };

    class IFrameGraphCompiler {
    public:
        virtual ~IFrameGraphCompiler() = default;
        // Backend builds native passes, CPU encode tasks and GPU submission plan; no acquire/dispatch/submit or shared ExecutePasses.
        // Success is non-null and retains IR/payload/resource pins; failure releases partial products but keeps the lease acquired.
        virtual FrameCompileResult Compile(std::shared_ptr<const FrozenFrameGraph> graph, const FrameCompileContext &context) = 0;
    };

} // namespace sky::aurora
