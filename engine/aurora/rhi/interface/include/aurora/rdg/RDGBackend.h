//
// Aurora RDG backend interface (per-backend compiler + executor).
//

#pragma once

namespace sky::aurora {

    class RenderGraph;
    class CommandBuffer;

    // Backend-specific half of RDG: barrier compiler + pass executor.
    // The graph structure, setup, and backend-agnostic analysis live in RenderGraph;
    // each backend implements this interface (see VulkanRDGBackend / D3D12RDGBackend /
    // MetalRDGBackend) and may reuse RenderGraph::DeriveBarriers / ExecutePasses.
    class RDGBackend {
    public:
        virtual ~RDGBackend() = default;

        virtual void CompileBarriers(RenderGraph &graph) = 0;
        virtual void Execute(RenderGraph &graph, CommandBuffer *cmdBuf) = 0;
    };

} // namespace sky::aurora
