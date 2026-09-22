//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletRigidBody.h>
#include <bullet/BulletCollisionObject.h>
#include <bullet/BulletCharacterController.h>
#include <bullet/BulletConversion.h>
#include <bullet/debug/BulletDebugDraw.h>

#include <framework/serialization/SerializationContext.h>
#include <core/profile/Profiler.h>

namespace sky::phy {

    void BulletPhysicsWorld::Reflect(SerializationContext *context)
    {
        context->Register<BulletPhysicsConfig>("BulletPhysicsConfig")
            .Member<&BulletPhysicsConfig::gravity>("gravity");
    }

    BulletPhysicsWorld::~BulletPhysicsWorld()
    {
        dynamicWorld  = nullptr;
        solver        = nullptr;
        broadPhase    = nullptr;
        dispatcher    = nullptr;
        configuration = nullptr;
    }

    BulletPhysicsWorld::BulletPhysicsWorld()
    {
        configuration = std::make_unique<btDefaultCollisionConfiguration>();
        dispatcher = std::make_unique<btCollisionDispatcher>(configuration.get());
        broadPhase = std::make_unique<btDbvtBroadphase>();
        solver = std::make_unique<btSequentialImpulseConstraintSolver>();

        broadPhase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());

        dynamicWorld = std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(),
            broadPhase.get(), solver.get(), configuration.get());
    }

    void BulletPhysicsWorld::Tick(float delta)
    {
        SKY_PROFILE_NAME("physics tick")
        if (dynamicWorld && enableSimulation) {
            dynamicWorld->stepSimulation(delta);

            if (enableDebugDraw) {
                dynamicWorld->debugDrawWorld();
            }
        }
    }

    void BulletPhysicsWorld::StartSimulation()
    {
        enableSimulation = true;
    }

    void BulletPhysicsWorld::StopSimulation()
    {
        enableSimulation = false;
    }

    void BulletPhysicsWorld::SetDebugDrawEnable(bool en)
    {
        enableDebugDraw = en;

        if (!debugDraw && enableDebugDraw) {
            debugDraw = std::make_unique<BulletDebugDraw>();
        }

        btIDebugDraw* bulletDebugDraw = enableDebugDraw ? static_cast<BulletDebugDraw*>(debugDraw.get()) : nullptr;
        dynamicWorld->setDebugDrawer(bulletDebugDraw);
    }

    void BulletPhysicsWorld::SetGravity(const Vector3 &gravity)
    {
        if (dynamicWorld) {
            dynamicWorld->setGravity(ToBullet(gravity));
        }
    }

    bool BulletPhysicsWorld::Raycast(const Vector3 &origin, const Vector3 &dir, float maxDistance, RaycastHit &out,
                                     const CollisionFilters *filter) const
    {
        const float length = dir.Length();
        if (!dynamicWorld || maxDistance <= 0.f || length <= 0.f) {
            return false;
        }

        const btVector3 from = ToBullet(origin);
        const btVector3 to   = ToBullet(origin + dir / length * maxDistance);

        btCollisionWorld::ClosestRayResultCallback callback(from, to);
        dynamicWorld->rayTest(from, to, callback);
        if (!callback.hasHit()) {
            return false;
        }

        auto *object = static_cast<CollisionObject *>(callback.m_collisionObject->getUserPointer());
        if (filter != nullptr && object != nullptr && (object->GetGroup() & *filter).value == 0) {
            return false;
        }

        out.object   = object;
        out.position = FromBullet(callback.m_hitPointWorld);
        out.normal   = FromBullet(callback.m_hitNormalWorld);
        out.distance = (out.position - origin).Length();
        return true;
    }

    bool BulletPhysicsWorld::Sweep(const BoxShape &shape, const Transform &from, const Vector3 &dir, float maxDistance,
                                   SweepResult &out, const CollisionFilters *filter) const
    {
        const float length = dir.Length();
        if (!dynamicWorld || maxDistance <= 0.f || length <= 0.f) {
            return false;
        }

        btBoxShape box(ToBullet(shape.halfExt));
        const btTransform start = ToBullet(from);
        btTransform end = start;
        end.getOrigin() += ToBullet(dir / length * maxDistance);

        btCollisionWorld::ClosestConvexResultCallback callback(start.getOrigin(), end.getOrigin());
        dynamicWorld->convexSweepTest(&box, start, end, callback);
        if (!callback.hasHit()) {
            return false;
        }

        auto *object = static_cast<CollisionObject *>(callback.m_hitCollisionObject->getUserPointer());
        if (filter != nullptr && object != nullptr && (object->GetGroup() & *filter).value == 0) {
            return false;
        }

        out.object   = object;
        out.position = FromBullet(callback.m_hitPointWorld);
        out.normal   = FromBullet(callback.m_hitNormalWorld);
        out.distance = (out.position - FromBullet(start.getOrigin())).Length();
        return true;
    }

    uint32_t BulletPhysicsWorld::Overlap(const BoxShape &shape, const Transform &pose, std::vector<OverlapResult> &out,
                                         const CollisionFilters *filter) const
    {
        if (!dynamicWorld) {
            return 0;
        }

        btBoxShape box(ToBullet(shape.halfExt));
        btCollisionObject probe;
        probe.setCollisionShape(&box);
        probe.setWorldTransform(ToBullet(pose));

        struct Callback : public btCollisionWorld::ContactResultCallback {
            std::vector<OverlapResult> *out    = nullptr;
            const CollisionFilters     *filter = nullptr;
            uint32_t                    count  = 0;

            btScalar addSingleResult(btManifoldPoint &, const btCollisionObjectWrapper *a, int, int,
                                     const btCollisionObjectWrapper *b, int, int) override
            {
                const btCollisionObject *candidate = (a->m_collisionObject->getUserPointer() != nullptr)
                                                         ? a->m_collisionObject
                                                         : b->m_collisionObject;
                auto *object = static_cast<CollisionObject *>(candidate->getUserPointer());
                if (object != nullptr && (filter == nullptr || (object->GetGroup() & *filter).value != 0)) {
                    out->push_back(OverlapResult{object});
                    ++count;
                }
                return 0.f;
            }
        } callback;
        callback.out    = &out;
        callback.filter = filter;

        dynamicWorld->contactTest(&probe, callback);
        return callback.count;
    }

    void BulletPhysicsWorld::OnAttachToWorld(World &world)
    {
    }

    void BulletPhysicsWorld::OnDetachFromWorld(World &world)
    {
    }

    void BulletPhysicsWorld::CollectDebugGeometry(PhysicsDebugGeometry &out) const
    {
        if (debugDraw != nullptr) {
            debugDraw->CollectGeometry(out);
        }
    }

    void BulletPhysicsWorld::AddRigidBodyImpl(RigidBody *rb)
    {
        auto *rigidBody = static_cast<BulletRigidBody*>(rb);
        rigidBody->SetPhysicsWorld(this);
    }

    void BulletPhysicsWorld::RemoveRigidBodyImpl(RigidBody *rb)
    {
        auto *rigidBody = static_cast<BulletRigidBody*>(rb);
        rigidBody->SetPhysicsWorld(nullptr);
    }

    void BulletPhysicsWorld::AddCollisionObjectImpl(CollisionObject *obj)
    {
        auto *colObj = static_cast<BulletCollisionObject*>(obj);
        colObj->SetPhysicsWorld(this);
    }

    void BulletPhysicsWorld::RemoveCollisionObjectImpl(CollisionObject *obj)
    {
        auto *colObj = static_cast<BulletCollisionObject*>(obj);
        colObj->SetPhysicsWorld(nullptr);
    }

    void BulletPhysicsWorld::AddCharacterControllerImpl(CharacterController *cc)
    {
        static_cast<BulletCharacterController *>(cc)->SetPhysicsWorld(this);
    }

    void BulletPhysicsWorld::RemoveCharacterControllerImpl(CharacterController *cc)
    {
        static_cast<BulletCharacterController *>(cc)->SetPhysicsWorld(nullptr);
    }
} // namespace sky::phy