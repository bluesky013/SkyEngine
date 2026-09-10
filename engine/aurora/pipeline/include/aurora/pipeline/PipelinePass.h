//
// Pipeline pass template base.
//

#pragma once

#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>
#include <aurora/pipeline/rg/RgBlockDesc.h>
#include <aurora/rhi/PipelineState.h>
#include <aurora/rhi/ResourceGroup.h>

#include <vector>

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
    //
    // Pass tier (set 1): subclass declares pass blocks via GetPassBlocks();
    // OnSetup creates the layout + persistent RG; OnSceneChanged rebuilds it.
    class PipelinePass {
    public:
        explicit PipelinePass(const Name &name) : mName(name) {}
        virtual ~PipelinePass() = default;

        PipelinePass(const PipelinePass &) = delete;
        PipelinePass &operator=(const PipelinePass &) = delete;

        virtual void OnSetup(Device *device);
        virtual void BuildRDG(RenderGraph &graph) = 0;
        virtual void OnSceneChanged();

        const Name &GetName() const { return mName; }

        ResourceGroup *GetPassResourceGroup() const { return mPassResourceGroup.Get(); }

    protected:
        // subclass declares pass-tier blocks (set 1); empty = no pass RG
        virtual const std::vector<RgBlockDesc> &GetPassBlocks() const { return mEmptyBlocks; }

        void RebuildPassResources(Device *device);

        Name mName;

        CounterPtr<GraphicsPipeline> mPSO;

    private:
        Device                          *mDevice = nullptr;
        ResourceGroupLayoutPtr           mPassLayout;
        ResourceGroupPtr                 mPassResourceGroup;
        static const std::vector<RgBlockDesc> mEmptyBlocks;
    };

} // namespace sky::aurora
