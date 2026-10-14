#pragma once

namespace sky::aurora {

    class RenderGraph;
    class CommandBuffer;
    class IFrameGraphCompiler;

    class RDGBackend {
    public:
        virtual ~RDGBackend() = default;

        virtual void CompileBarriers(RenderGraph &graph) = 0;
        virtual void Execute(RenderGraph &graph, CommandBuffer *cmdBuf) = 0;

        // nullptr means legacy-only; new contexts must not fall back to CompileBarriers.
        virtual IFrameGraphCompiler *GetFrameGraphCompiler() noexcept { return nullptr; }

    };

} // namespace sky::aurora
