//
// Created on 2026/09/22.
//

#include <physics/PhysicsMaterial.h>
#include <physics/PhysicsRegistry.h>

namespace sky::phy {

    PhysicsMaterial::PhysicsMaterial(const PhysicsMaterialData &inData) : data(inData)
    {
        impl.reset(PhysicsRegistry::Get()->CreateMaterial(data));
    }

    PhysicsMaterial::~PhysicsMaterial() = default;

} // namespace sky::phy
