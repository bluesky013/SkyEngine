#pragma once

namespace sky::aurora {

    class RenderGraph;
    class CommandBuffer;

    class RDGBackend {
    public:
        virtual ~RDGBackend() = default;

        virtual void CompileBarriers(RenderGraph &graph) = 0;
        virtual void Execute(RenderGraph &graph, CommandBuffer *cmdBuf) = 0;

    };

} // namespace sky::aurora
