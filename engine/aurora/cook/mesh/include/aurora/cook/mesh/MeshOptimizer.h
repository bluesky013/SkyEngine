//
// MeshOptimizer: meshopt vertex cache / overdraw / vertex fetch optimization
// on the interleaved stream. Vertex order is remapped; sub-mesh index ranges
// and the attribute layout are preserved.
//

#pragma once

#include <aurora/cook/mesh/MeshProcess.h>

namespace sky::aurora::cook {

    class MeshOptimizer : public MeshProcess {
    public:
        struct Payload {
            CookedMesh *mesh = nullptr;
        };

        explicit MeshOptimizer(const Payload &pd) : payload(pd) {}
        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::aurora::cook
