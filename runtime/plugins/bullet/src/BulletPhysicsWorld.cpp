//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletRigidBody.h>
#include <bullet/BulletConversion.h>

#include <bullet/debug/BulletDebugDraw.h>
namespace sky::phy {

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

        dynamicWorld = std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(),
            broadPhase.get(), solver.get(), configuration.get());
    }

    void BulletPhysicsWorld::Tick(float delta)
    {
        if (dynamicWorld && enableSimulation) {
            dynamicWorld->stepSimulation(delta);
        }
    }

    void BulletPhysicsWorld::SetSimulationEnable(bool en)
    {
        enableSimulation = en;
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

    void BulletPhysicsWorld::AddRigidBodyImpl(RigidBody *rb)
    {
        auto *rigidBody = static_cast<BulletRigidBody*>(rb);
        auto *btRb = rigidBody->GetRigidBody();
        if (btRb != nullptr) {
            dynamicWorld->addRigidBody(btRb, rb->GetGroup(), rb->GetMask());
        }
        rigidBody->SetPhysicsWorld(this);
    }

    void BulletPhysicsWorld::RemoveRigidBodyImpl(RigidBody *rb)
    {
        auto *rigidBody = static_cast<BulletRigidBody*>(rb);
        auto *btRb = rigidBody->GetRigidBody();
        if (btRb != nullptr) {
            dynamicWorld->removeRigidBody(btRb);
        }
    }

    void BulletPhysicsWorld::AddCharacterControllerImpl(CharacterController *rb)
    {

    }

    void BulletPhysicsWorld::RemoveCharacterControllerImpl(CharacterController *rb)
    {

    }
} // namespace sky::phy