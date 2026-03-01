//
// Created by blues on 2024/10/6.
//

#include <physics/PhysicsShape.h>
#include <physics/PhysicsRegistry.h>

namespace sky::phy {

    CounterPtr<TriangleMesh> PhysicsShape::GetTriangleMesh() const
    {
        return impl->GetTriangleMesh();
    }

    PhysicsBoxShape::PhysicsBoxShape(const BoxShape &shape)
    {
        impl.reset(PhysicsRegistry::Get()->CreateBox(shape));
    }

    PhysicsSphereShape::PhysicsSphereShape(const SphereShape &shape)
    {
        impl.reset(PhysicsRegistry::Get()->CreateSphere(shape));
    }

    PhysicsTriangleMeshShape::PhysicsTriangleMeshShape(const TriangleMeshShape &shape)
    {
        impl.reset(PhysicsRegistry::Get()->CreateTriangleMesh(shape));
    }
} // namespace sky::phy