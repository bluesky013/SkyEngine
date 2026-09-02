//
// D3D12 RDG backend (compiler + executor).
//

#include "D3D12RDGBackend.h"

#include <aurora/rdg/RenderGraph.h>

namespace sky::aurora {

    void D3D12RDGBackend::CompileBarriers(RenderGraph &graph)
    {
        // D3D12 barrier derivation; v1 reuses the shared walk. D3D12-only
        // optimizations (resource state promotion/decay elision) belong here.
        graph.DeriveBarriers();
    }

    void D3D12RDGBackend::Execute(RenderGraph &graph, CommandBuffer *cmdBuf)
    {
        // D3D12 pass emission; v1 reuses the shared encoder walk.
        graph.ExecutePasses(cmdBuf);
    }

} // namespace sky::aurora
