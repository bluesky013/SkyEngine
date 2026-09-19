//
// MeshAssembler: CookMeshSource -> CookedMesh. Bakes node transforms, picks
// the attribute set, and interleaves everything into a single vertex stream.
//

#pragma once

#include <aurora/cook/mesh/MeshBuildConfig.h>
#include <aurora/cook/mesh/MeshProcess.h>

namespace sky::aurora::cook {

    class MeshAssembler : public MeshProcess {
    public:
        struct Payload {
            const CookMeshSource  *source = nullptr;
            const MeshBuildConfig *config = nullptr;
            CookedMesh            *out    = nullptr;
        };

        explicit MeshAssembler(const Payload &pd) : payload(pd) {}
        void DoWork() override;

    private:
        Payload payload;
    };

} // namespace sky::aurora::cook
