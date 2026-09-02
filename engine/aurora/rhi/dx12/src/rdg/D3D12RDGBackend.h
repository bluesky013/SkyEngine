//
// D3D12 RDG backend (compiler + executor).
//

#pragma once

#include <aurora/rdg/RDGBackend.h>

namespace sky::aurora {

    class D3D12RDGBackend : public RDGBackend {
    public:
        void CompileBarriers(RenderGraph &graph) override;
        void Execute(RenderGraph &graph, CommandBuffer *cmdBuf) override;
    };

} // namespace sky::aurora
