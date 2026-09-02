//
// Metal RDG backend (compiler + executor).
//

#include "MetalRDGBackend.h"

#include <aurora/rdg/RenderGraph.h>

namespace sky::aurora {

    void MetalRDGBackend::CompileBarriers(RenderGraph &graph)
    {
        // Metal barrier derivation. Metal has no explicit layout transitions;
        // for v1 this reuses the shared walk and relies on encoder-boundary
        // implicit sync, with memoryBarrier handled by MetalCommandBuffer.
        graph.DeriveBarriers();
    }

    void MetalRDGBackend::Execute(RenderGraph &graph, CommandBuffer *cmdBuf)
    {
        // Metal pass emission; v1 reuses the shared encoder walk.
        graph.ExecutePasses(cmdBuf);
    }

} // namespace sky::aurora
