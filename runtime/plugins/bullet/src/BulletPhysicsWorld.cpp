//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletRigidBody.h>
#include <bullet/BulletCollisionObject.h>
#include <bullet/BulletConversion.h>
#include <bullet/debug/BulletDebugDraw.h>

#include <framework/serialization/SerializationContext.h>
#include <core/profile/Profiler.h>

#include <render/adaptor/RenderSceneProxy.h>

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
            debugDraw->SetTechnique(debugTech);
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

    void BulletPhysicsWorld::OnAttachToWorld(World &world)
    {
        if (debugDraw != nullptr) {
            auto *renderScene = static_cast<RenderSceneProxy*>(world.GetSubSystem("RenderScene"))->GetRenderScene();
            renderScene->AddPrimitive(static_cast<BulletDebugDraw*>(debugDraw.get())->GetPrimitive());
        }
    }
    void BulletPhysicsWorld::OnDetachFromWorld(World &world)
    {
        if (debugDraw != nullptr) {
            auto *renderScene = static_cast<RenderSceneProxy*>(world.GetSubSystem("RenderScene"))->GetRenderScene();
            renderScene->RemovePrimitive(static_cast<BulletDebugDraw*>(debugDraw.get())->GetPrimitive());
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

    void BulletPhysicsWorld::AddCharacterControllerImpl(CharacterController *rb)
    {
    }

    void BulletPhysicsWorld::RemoveCharacterControllerImpl(CharacterController *rb)
    {

    }
} // namespace sky::phy