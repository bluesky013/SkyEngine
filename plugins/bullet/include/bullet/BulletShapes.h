//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/PhysicsBase.h>
#include <physics/PhysicsMaterial.h>
#include <btBulletCollisionCommon.h>
#include <core/shapes/TriangleMesh.h>

namespace sky::phy {

    class BulletMaterial : public IMaterialImpl {
    public:
        explicit BulletMaterial(const PhysicsMaterialData &inData) : data(inData) {}
        const PhysicsMaterialData &GetData() const { return data; }

    private:
        PhysicsMaterialData data;
    };

    struct TriangleMeshWrap {
        void Set(const CounterPtr<TriangleMesh> &mesh);
        CounterPtr<TriangleMesh> triangle;
        std::unique_ptr<btTriangleIndexVertexArray> meshInterface;
    };

    class BulletShape : public IShapeImpl {
    public:
        explicit BulletShape(const BoxShape &box);
        explicit BulletShape(const SphereShape &sphere);
        explicit BulletShape(const TriangleMeshShape &shape);
        explicit BulletShape(const HeightFieldShape &shape);
        explicit BulletShape(const CapsuleShape &shape);

        ~BulletShape() override = default;

        btCollisionShape* GetShape() const { return collisionShape.get(); }

        CounterPtr<TriangleMesh> GetTriangleMesh() const override;

    protected:
        std::unique_ptr<btCollisionShape> collisionShape;
        std::unique_ptr<btCollisionShape> baseShape;
        TriangleMeshWrap triangleMesh;
        std::vector<float> heightFieldData;
    };
} // namespace sky::phy