//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <framework/world/World.h>
#include <framework/serialization/SerializationContext.h>

#include <core/event/Event.h>

#include <physics/PhysicsRegistry.h>
#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletCharacterController.h>
#include <bullet/BulletRigidBody.h>
#include <physics/components/RigidBodyComponent.h>


namespace sky::phy {

    class BulletFactory : public PhysicsRegistry::Impl {
    public:
        BulletFactory() = default;
        ~BulletFactory() override = default;

        PhysicsWorld* CreatePhysicsWorld() override
        {
            return new BulletPhysicsWorld();
        }

        RigidBody* CreateRigidBody() override
        {
            return new BulletRigidBody();
        }

        CharacterController* CreateCharacterController() override
        {
            return new BulletCharacterController();
        }
    };

    class BulletPhysicsModule : public IModule, public IWorldEvent {
    public:
        BulletPhysicsModule() = default;
        ~BulletPhysicsModule() override = default;

        void OnCreateWorld(World& world) override
        {
            if (world.CheckSystem(PhysicsWorld::NAME.data())) {
                world.AddSubSystem(phy::PhysicsWorld::NAME.data(), new BulletPhysicsWorld());
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

            RigidBodyComponent::Reflect(SerializationContext::Get());
        }

        void Shutdown() override
        {
            PhysicsRegistry::Get()->UnRegister();
        }

        EventBinder<IWorldEvent> worldEvent;
    };
} // namespace sky::phy
REGISTER_MODULE(sky::phy::BulletPhysicsModule)