//
// Created on 2026/09/22.
//

#pragma once

#include <core/shapes/TriangleMesh.h>
#include <core/util/Uuid.h>

#include <functional>
#include <memory>
#include <string>

namespace sky::phy {

    // Interface seam: converts a mesh asset into physics TriangleMesh data. Implemented by a render
    // bridge (plugins/jolt/render) so the physics interface stays free of render dependencies.
    using PhysicsMeshProvider = std::function<CounterPtr<TriangleMesh>(const Uuid &asset)>;

    void SetPhysicsMeshProvider(PhysicsMeshProvider provider, std::shared_ptr<const void> codeLease = {}, std::string assetType = {});
    CounterPtr<TriangleMesh> CreatePhysicsMesh(const Uuid &asset);
    std::string              GetPhysicsMeshAssetType();

} // namespace sky::phy
