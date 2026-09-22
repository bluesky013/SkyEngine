//
// Created on 2026/09/22.
//

#include <physics/PhysicsMesh.h>

#include <utility>

namespace sky::phy {

    namespace {
        PhysicsMeshProvider gMeshProvider;
    }

    void SetPhysicsMeshProvider(PhysicsMeshProvider provider)
    {
        gMeshProvider = std::move(provider);
    }

    CounterPtr<TriangleMesh> CreatePhysicsMesh(const Uuid &asset)
    {
        return gMeshProvider ? gMeshProvider(asset) : nullptr;
    }

} // namespace sky::phy
