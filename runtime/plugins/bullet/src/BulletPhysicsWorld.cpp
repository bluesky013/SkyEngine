//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletRigidBody.h>
#include <bullet/BulletDebugDraw.h>
#include <bullet/BulletConversion.h>

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
        ProcessPendingTasks();

        if (dynamicWorld && enableSimulation) {
            dynamicWorld->stepSimulation(delta);
        }
    }

    void BulletPhysicsWorld::ProcessPendingTasks()
    {
        if (!dynamicWorld) {
            return;
        }

        for (auto &task : pendingTasks) {
            auto *rb = static_cast<BulletRigidBody*>(task.rigidBody);
            switch (task.op) {
                case BuildOperation::BUILD:
                    dynamicWorld->addRigidBody(rb->BuildRigidBody(), rb->GetGroup(), rb->GetMask());
                    break;
                case BuildOperation::DESTROY:
                    dynamicWorld->removeRigidBody(rb->GetRigidBody());
                    break;
            }
        }
        pendingTasks.clear();
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
} // namespace sky::phy