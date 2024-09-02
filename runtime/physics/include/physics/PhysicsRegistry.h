//
// Created by blues on 2024/9/2.
//

#pragma once

#include <core/environment/Singleton.h>

namespace sky::phy {
    class PhysicsWorld;
    class CharacterController;

    class PhysicsRegistry : public Singleton<PhysicsRegistry> {
    public:
        PhysicsRegistry() = default;
        ~PhysicsRegistry() override = default;

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual PhysicsWorld* CreatePhysicsWorld() = 0;
            virtual CharacterController* CreateCharacterController() = 0;
        };

        PhysicsWorld* CreatePhysicsWorld();
        CharacterController* CreateCharacterController();

        void Register(Impl* factory);
        void UnRegister();

    private:
        std::unique_ptr<Impl> factory;
    };

} // namespace sky::phy