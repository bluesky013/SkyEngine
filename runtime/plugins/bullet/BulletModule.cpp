//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <framework/world/World.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <core/event/Event.h>

#include <render/adaptor/assets/TechniqueAsset.h>

#include <physics/PhysicsRegistry.h>
#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletCharacterController.h>
#include <bullet/BulletRigidBody.h>
#include <bullet/BulletShapes.h>
#include <bullet/BulletCollisionObject.h>
#include <physics/components/RigidBodyComponent.h>
#include <physics/components/CollisionComponent.h>

namespace sky::phy {

    class BulletFactory : public PhysicsRegistry::Impl {
    public:
        BulletFactory()
        {
            auto techAsset = AssetManager::Get()->LoadAssetFromPath<Technique>("techniques/debug.tech");
            techAsset->BlockUntilLoaded();
            debugTech = CreateTechniqueFromAsset(techAsset);
        }
        ~BulletFactory() override = default;

        PhysicsWorld* CreatePhysicsWorld() override
        {
            auto *world = new BulletPhysicsWorld();
            world->SetTechnique(debugTech);
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

    private:
        CounterPtr<Technique> debugTech;
    };

    class BulletPhysicsModule : public IModule, public IWorldEvent {
    public:
        BulletPhysicsModule() = default;
        ~BulletPhysicsModule() override = default;

        void OnCreateWorld(World& world) override
        {
            if (world.CheckSystem(PhysicsWorld::NAME.data())) {
                world.AddSubSystem(phy::PhysicsWorld::NAME.data(), PhysicsRegistry::Get()->CreatePhysicsWorld());
            }
        }

        bool Init(const StartArguments &args) override
        {
            worldEvent.Bind(this);
            return true;
        }

        void Start() override
        {
            PhysicsRegistry::Get()->Register(new BulletFactory());

            auto *context = SerializationContext::Get();
            PhysicsRegistry::Reflect(context);
            RigidBodyComponent::Reflect(context);
            CollisionComponent::Reflect(context);
        }

        void Shutdown() override
        {
            PhysicsRegistry::Get()->UnRegister();
        }
        EventBinder<IWorldEvent> worldEvent;
    };
} // namespace sky::phy
REGISTER_MODULE(sky::phy::BulletPhysicsModule)