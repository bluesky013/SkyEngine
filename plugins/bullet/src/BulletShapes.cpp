//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletShapes.h>
#include <bullet/BulletConversion.h>

#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

namespace sky::phy {

    void TriangleMeshWrap::Set(const CounterPtr<TriangleMesh> &mesh)
    {
        triangle = mesh;
        if (triangle == nullptr) {
            return;
        }

        meshInterface = std::make_unique<btTriangleIndexVertexArray>();

        auto idxStride = mesh->indexType == IndexType::U32 ? sizeof(uint32_t) : sizeof(uint16_t);

        for (auto &view : mesh->views) {
            btIndexedMesh idxMesh = {};
            idxMesh.m_numTriangles = static_cast<int>(view.numTris);
            idxMesh.m_numVertices  = static_cast<int>(view.numVert);
            idxMesh.m_vertexStride = static_cast<int>(mesh->vtxStride);
            idxMesh.m_triangleIndexStride = static_cast<int>(3 * idxStride);

            const auto* vertices = &reinterpret_cast<const float*>(mesh->position.data())[view.firstVertex];
            const uint8_t *tri;
            if (mesh->indexType == IndexType::U32) {
                const int* tris = &reinterpret_cast<const int*>(mesh->indexRaw.data())[view.firstIndex];
                tri = reinterpret_cast<const uint8_t*>(tris);
            } else {
                const int16_t* tris = &reinterpret_cast<const int16_t *>(mesh->indexRaw.data())[view.firstIndex];
                tri = reinterpret_cast<const uint8_t*>(tris);
            }
            
            idxMesh.m_vertexBase = reinterpret_cast<const uint8_t *>(vertices);
            idxMesh.m_triangleIndexBase = tri;

            meshInterface->addIndexedMesh(idxMesh, ToBullet(mesh->indexType));
        }
    }

    BulletShape::BulletShape(const BoxShape &shape)
    {
        auto *boxShape = new btBoxShape(ToBullet(shape.halfExt));
        if (shape.pivot.x == 0.f && shape.pivot.y == 0.f && shape.pivot.z == 0.f) {
            collisionShape.reset(boxShape);
        } else {
            baseShape.reset(boxShape);
            auto* compound = new btCompoundShape();
            btTransform trans{btQuaternion::getIdentity(), ToBullet(shape.pivot)};
            compound->addChildShape(trans, baseShape.get());
            collisionShape.reset(compound);
        }
    }

    BulletShape::BulletShape(const SphereShape &shape)
    {
        auto *sphereShape = new btSphereShape(shape.radius);
        if (shape.pivot.x == 0.f && shape.pivot.y == 0.f && shape.pivot.z == 0.f) {
            collisionShape.reset(sphereShape);
        } else {
            baseShape.reset(sphereShape);
            auto* compound = new btCompoundShape();
            btTransform trans{btQuaternion::getIdentity(), ToBullet(shape.pivot)};
            compound->addChildShape(trans, baseShape.get());
            collisionShape.reset(compound);
        }
    }

    BulletShape::BulletShape(const TriangleMeshShape &mesh)
    {
        triangleMesh.Set(mesh.mesh);
        if (triangleMesh.meshInterface != nullptr) {
            collisionShape = std::make_unique<btBvhTriangleMeshShape>(triangleMesh.meshInterface.get(), true);
        }
    }

    CounterPtr<TriangleMesh> BulletShape::GetTriangleMesh() const
    {
        return triangleMesh.triangle;
    }

    BulletShape::BulletShape(const HeightFieldShape &shape)
    {
        // Bullet reads from the sample buffer for the shape's lifetime; keep an owned copy.
        heightFieldData = shape.samples;

        const auto width     = static_cast<int>(shape.width);
        const auto height    = static_cast<int>(shape.height);
        const auto upAxis    = static_cast<int>(shape.upAxis);
        const auto hScale    = shape.heightScale != 0.f ? shape.heightScale : 1.f;
        const auto minHeight = shape.minHeight;
        const auto maxHeight = shape.maxHeight > shape.minHeight ? shape.maxHeight : shape.minHeight;

        auto *heightField = new btHeightfieldTerrainShape(
            width, height, heightFieldData.data(), hScale,
            minHeight, maxHeight, upAxis, PHY_FLOAT, false);

        // Bullet heightfields are centered on the origin with unit grid spacing; scale to world size.
        heightField->setLocalScaling(btVector3(shape.scaleX, 1.f, shape.scaleZ));
        collisionShape.reset(heightField);
    }

    BulletShape::BulletShape(const CapsuleShape &shape)
    {
        auto *capsuleShape = new btCapsuleShape(shape.radius, shape.height);
        if (shape.pivot.x == 0.f && shape.pivot.y == 0.f && shape.pivot.z == 0.f) {
            collisionShape.reset(capsuleShape);
        } else {
            baseShape.reset(capsuleShape);
            auto* compound = new btCompoundShape();
            btTransform trans{btQuaternion::getIdentity(), ToBullet(shape.pivot)};
            compound->addChildShape(trans, baseShape.get());
            collisionShape.reset(compound);
        }
    }

} // namespace sky::phy