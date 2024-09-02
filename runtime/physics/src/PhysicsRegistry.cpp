//
// Created by blues on 2024/9/2.
//

#include <physics/PhysicsRegistry.h>

namespace sky::phy {

    PhysicsWorld* PhysicsRegistry::CreatePhysicsWorld()
    {
        return factory ? factory->CreatePhysicsWorld() : nullptr;
    }

    CharacterController* PhysicsRegistry::CreateCharacterController()
    {
        return factory ? factory->CreateCharacterController() : nullptr;
    }

    void PhysicsRegistry::Register(Impl* impl)
    {
        factory.reset(impl);
    }

    void PhysicsRegistry::UnRegister()
    {
        factory.reset();
    }
} // namespace sky::phy