//
// Aurora render primitive implementation.
//

#include <aurora/pipeline/scene/RenderPrimitive.h>

namespace sky::aurora {

    void RenderPrimitive::GatherRenderItem(GatherContext &ctx) const
    {
        auto it = techniques.find(ctx.tag);
        if (it == techniques.end()) {
            if (ctx.tag == Name{}) {
                it = techniques.begin(); // empty tag: fall back to the default (first) binding
            }
            if (it == techniques.end()) {
                return;
            }
        }

        const TechniqueBinding &binding = it->second;
        ctx.item.pso                = binding.pso;
        ctx.item.batchResourceGroup = binding.batchResourceGroup;
        ctx.item.vb                 = vb;
        ctx.item.ib                 = ib;
        ctx.item.vbOffset           = vbOffset;
        ctx.item.ibOffset           = ibOffset;
        ctx.item.args               = args;
        ctx.gathered                = true;
    }

} // namespace sky::aurora
