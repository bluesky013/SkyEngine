//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletShapes.h>
#include <bullet/BulletConversion.h>
#include <render/adaptor/assets/MeshAsset.h>

namespace sky::phy {

    void TriangleMeshWrap::Set(const CounterPtr<TriangleMesh> &mesh)
    {
        triangle = mesh;
        meshInterface = std::make_unique<btTriangleIndexVertexArray>();

        auto idxStride = mesh->indexType == IndexType::U32 ? sizeof(uint32_t) : sizeof(uint16_t);

        for (auto &view : mesh->views) {
            btIndexedMesh idxMesh = {};
            idxMesh.m_numTriangles = static_cast<int>(view.numTris);
            idxMesh.m_numVertices  = static_cast<int>(view.numVert);
            idxMesh.m_vertexStride = static_cast<int>(mesh->vtxStride);
            idxMesh.m_triangleIndexStride = static_cast<int>(3 * idxStride);

            idxMesh.m_vertexBase = view.vertexBase;
            idxMesh.m_triangleIndexBase = view.triBase;

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
        auto *am = AssetManager::Get();

        auto meshAsset = am->LoadAsset<Mesh>(mesh.asset);
        meshAsset->BlockUntilLoaded();
        triangleMesh.Set(CreateTriangleMesh(meshAsset));

        collisionShape = std::make_unique<btBvhTriangleMeshShape>(triangleMesh.meshInterface.get(), true);
    }

    CounterPtr<TriangleMesh> BulletShape::GetTriangleMesh() const
    {
        return triangleMesh.triangle;
    }

} // namespace sky::phy