//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <framework/world/World.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <core/event/Event.h>

#include <physics/PhysicsRegistry.h>
#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletCharacterController.h>
#include <bullet/BulletRigidBody.h>
#include <bullet/BulletShapes.h>
#include <bullet/BulletCollisionObject.h>
#include <physics/components/RigidBodyComponent.h>
#include <physics/components/CollisionComponent.h>
#include <bullet/BulletRegistry.h>

namespace sky::phy {

    class BulletFactory : public PhysicsRegistry::Impl {
    public:
        BulletFactory() = default;
        ~BulletFactory() override = default;

        PhysicsWorld* CreatePhysicsWorld() override
        {
            auto *world = new BulletPhysicsWorld();
            world->SetDebugDrawEnable(true);
            return world;
        }

        RigidBody* CreateRigidBody() override
        {
            return new BulletRigidBody();
        }

        CharacterController* CreateCharacterController() override
        {
            return new BulletCharacterController();
        }

        CollisionObject* CreateCollisionObject() override
        {
            return new BulletCollisionObject();
        }

        IShapeImpl* CreateBox(const BoxShape& shape) override
        {
            return new BulletShape(shape);
        }

        IShapeImpl* CreateSphere(const SphereShape& shape) override
        {
            return new BulletShape(shape);
        }

        IShapeImpl* CreateTriangleMesh(const TriangleMeshShape& shape) override
        {
            return new BulletShape(shape);
        }

        IShapeImpl* CreateHeightField(const HeightFieldShape& shape) override
        {
            return new BulletShape(shape);
        }

        IShapeImpl* CreateCapsule(const CapsuleShape& shape) override
        {
            return new BulletShape(shape);
        }

        IMaterialImpl* CreateMaterial(const PhysicsMaterialData& data) override
        {
            return new BulletMaterial(data);
        }
    };

    class BulletPhysicsModule : public IModule {
    public:
        BulletPhysicsModule() = default;
        ~BulletPhysicsModule() override = default;

        bool Init(const StartArguments &args) override
        {
            return true;
        }

        void Start() override
        {
            RegisterBulletPhysics();

            auto *context = SerializationContext::Get();
            PhysicsRegistry::Reflect(context);
            RigidBodyComponent::Reflect(context);
            CollisionComponent::Reflect(context);
        }

        void Shutdown() override
        {
            UnregisterBulletPhysics();
        }
    };

    void RegisterBulletPhysics()
    {
        PhysicsRegistry::Get()->Register(new BulletFactory());
    }

    void UnregisterBulletPhysics()
    {
        PhysicsRegistry::Get()->UnRegister();
    }
} // namespace sky::phy
REGISTER_MODULE(sky::phy::BulletPhysicsModule)