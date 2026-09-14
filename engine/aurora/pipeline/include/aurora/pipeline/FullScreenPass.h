//
// FullScreenPass: base template for fullscreen passes.
//
// Owns the input/target RDG handles and provides a shared declaration that
// emits a FullScreenPass RDG node (target color attachment + input SRV + pass
// resource group + PSO). Concrete passes (e.g. TextureToScreenPass) override
// BuildRDG / OnSetup / GetPassBlocks as needed.
//

#pragma once

#include <aurora/pipeline/PipelinePass.h>
#include <aurora/rdg/RDGHandles.h>

namespace sky::aurora {

    class RenderGraph;

    class FullScreenPass : public PipelinePass {
    public:
        explicit FullScreenPass(const Name &name) : PipelinePass(name) {}
        ~FullScreenPass() override = default;

        void SetInput(RDGTextureHandle handle)  { mInput  = handle; }
        void SetTarget(RDGTextureHandle handle) { mTarget = handle; }

        RDGTextureHandle GetInput() const  { return mInput; }
        RDGTextureHandle GetTarget() const { return mTarget; }

    protected:
        // Declare the fullscreen pass node (no-op if input/target invalid).
        void BuildFullScreenPass(RenderGraph &graph,
                                 LoadOp loadOp = LoadOp::DONT_CARE,
                                 StoreOp storeOp = StoreOp::STORE);

        RDGTextureHandle mInput;
        RDGTextureHandle mTarget;
    };

} // namespace sky::aurora
