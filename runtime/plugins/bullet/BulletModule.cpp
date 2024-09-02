//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <physics/PhysicsRegistry.h>
#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletCharacterController.h>

namespace sky::phy {

    class BulletFactory : public PhysicsRegistry::Impl {
    public:
        BulletFactory() = default;
        ~BulletFactory() override = default;

        PhysicsWorld* CreatePhysicsWorld() override
        {
            return new BulletPhysicsWorld();
        }

        CharacterController* CreateCharacterController() override
        {
            return new BulletCharacterController();
        }
    };

    class BulletPhysicsModule : public IModule {
    public:
        BulletPhysicsModule() = default;
        ~BulletPhysicsModule() override = default;

        void Start() override
        {
            PhysicsRegistry::Get()->Register(new BulletFactory());
        }

        void Shutdown() override
        {
            PhysicsRegistry::Get()->UnRegister();
        }
    };
} // namespace sky::phy
REGISTER_MODULE(sky::phy::BulletPhysicsModule)