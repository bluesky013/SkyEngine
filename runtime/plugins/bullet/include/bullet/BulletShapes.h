//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/PhysicsBase.h>
#include <btBulletCollisionCommon.h>

namespace sky::phy {

    class BulletShape : public IShapeImpl {
    public:
        explicit BulletShape(const BoxShape &box);
        explicit BulletShape(const SphereShape &sphere);
        explicit BulletShape(const TriangleMeshShape &shape);

        ~BulletShape() override = default;

        btCollisionShape* GetShape() const { return collisionShape.get(); }

    protected:
        std::unique_ptr<btCollisionShape> collisionShape;
        std::unique_ptr<btCollisionShape> baseShape;

        std::unique_ptr<btTriangleIndexVertexArray> meshInterface;
    };
} // namespace sky::phy