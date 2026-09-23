//
// Created on 2026/09/22.
//

#pragma once

#include <core/shapes/TriangleMesh.h>
#include <core/util/Uuid.h>

#include <functional>

namespace sky::phy {

    // Interface seam: converts a mesh asset into physics TriangleMesh data. Implemented by a render
    // bridge (plugins/bullet/render) so the physics core stays free of render dependencies.
    using PhysicsMeshProvider = std::function<CounterPtr<TriangleMesh>(const Uuid &asset)>;

    void SetPhysicsMeshProvider(PhysicsMeshProvider provider);
    CounterPtr<TriangleMesh> CreatePhysicsMesh(const Uuid &asset);

} // namespace sky::phy
