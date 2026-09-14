//
// FullScreenPass base implementation.
//

#include <aurora/pipeline/FullScreenPass.h>
#include <aurora/rdg/RenderGraph.h>

namespace sky::aurora {

    void FullScreenPass::BuildFullScreenPass(RenderGraph &graph, LoadOp loadOp, StoreOp storeOp)
    {
        if (!mInput.IsValid() || !mTarget.IsValid()) {
            return;
        }

        graph.AddFullScreenPass(mName,
            [this, loadOp, storeOp](FullScreenPassBuilder &builder) {
                builder.SetTarget(mTarget, loadOp, storeOp);
                builder.SetInputSRV(mInput);

                if (GetPassResourceGroup() != nullptr) {
                    builder.SetPassResourceGroup(GetPassResourceGroup());
                }
                if (mPSO) {
                    builder.SetTechnique(mPSO.Get());
                }
            });
    }

} // namespace sky::aurora
