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

        ~BulletShape() override = default;

        btCollisionShape* GetShape() const { return collisionShape.get(); }

    protected:
        std::unique_ptr<btCollisionShape> collisionShape;
        std::unique_ptr<btCollisionShape> baseShape;
    };
} // namespace sky::phy