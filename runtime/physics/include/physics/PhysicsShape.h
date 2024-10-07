//
// Created by blues on 2024/10/6.
//

#pragma once

#include <physics/PhysicsBase.h>

namespace sky::phy {

    class PhysicsShape {
    public:
        PhysicsShape() = default;
        virtual ~PhysicsShape() = default;

        virtual IShapeImpl* GetShape() const { return impl.get(); }
    protected:
        std::unique_ptr<IShapeImpl> impl;
    };

    class PhysicsBoxShape : public PhysicsShape {
    public:
        explicit PhysicsBoxShape(const BoxShape &shape);
        ~PhysicsBoxShape() override = default;
    };

    class PhysicsSphereShape : public PhysicsShape {
    public:
        explicit PhysicsSphereShape(const SphereShape &shape);
        ~PhysicsSphereShape() override = default;
    };

    class PhysicsTriangleMeshShape : public PhysicsShape {
    public:
        explicit PhysicsTriangleMeshShape(const TriangleMeshShape &shape);
        ~PhysicsTriangleMeshShape() override = default;
    };

} // namespace sky::phy