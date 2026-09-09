//
// Pipeline pass template base.
//

#pragma once

#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>
#include <aurora/rhi/PipelineState.h>
#include <aurora/rhi/ResourceGroup.h>

namespace sky::aurora {

    class Device;
    class RenderGraph;

    // PipelinePass: owns persistent resources (PSO / ResourceGroup), rebuilds
    // RDG nodes every frame. The RDG layer never caches or manages their lifetime.
    //
    // Lifecycle:
    //   OnSetup(device)      -- one-time: create persistent PSO / ResourceGroup
    //   BuildRDG(graph)      -- per-frame: declare resources + passes + queues + items
    //   OnSceneChanged()     -- explicit persistent resource rebuild
    class PipelinePass {
    public:
        explicit PipelinePass(const Name &name) : mName(name) {}
        virtual ~PipelinePass() = default;

        PipelinePass(const PipelinePass &) = delete;
        PipelinePass &operator=(const PipelinePass &) = delete;

        virtual void OnSetup(Device *device) { (void)device; }
        virtual void BuildRDG(RenderGraph &graph) = 0;
        virtual void OnSceneChanged() {}

        const Name &GetName() const { return mName; }

    protected:
        Name mName;

        CounterPtr<GraphicsPipeline> mPSO;
        ResourceGroup               *mPassResourceGroup = nullptr;
    };

} // namespace sky::aurora
