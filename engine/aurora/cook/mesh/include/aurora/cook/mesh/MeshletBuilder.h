//
// MeshletBuilder: meshopt meshlet construction + bounds on the cooked mesh.
//

#pragma once

#include <aurora/cook/mesh/MeshProcess.h>

namespace sky::aurora::cook {

    class MeshletBuilder : public MeshProcess {
    public:
        struct Payload {
            CookedMesh *mesh         = nullptr;
            uint32_t    maxVertices  = 64;
            uint32_t    maxTriangles = 124;
            float       coneWeight   = 0.f;
        };

        explicit MeshletBuilder(const Payload &pd) : payload(pd) {}
        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::aurora::cook
