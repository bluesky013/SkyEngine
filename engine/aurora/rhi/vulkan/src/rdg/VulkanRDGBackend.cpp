//
// Vulkan RDG backend (compiler + executor).
//

#include "VulkanRDGBackend.h"

#include <aurora/rdg/RenderGraph.h>

namespace sky::aurora {

    void VulkanRDGBackend::CompileBarriers(RenderGraph &graph)
    {
        // Vulkan barrier derivation. For v1 this reuses the shared access-chain
        // walk; Vulkan-only optimizations (vkCmdPipelineBarrier2 batching,
        // subpass/render-pass merging) belong here.
        graph.DeriveBarriers();
    }

    void VulkanRDGBackend::Execute(RenderGraph &graph, CommandBuffer *cmdBuf)
    {
        // Vulkan pass emission; v1 reuses the shared encoder walk.
        graph.ExecutePasses(cmdBuf);
    }

} // namespace sky::aurora
