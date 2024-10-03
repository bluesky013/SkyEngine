//
// Created by blues on 2024/9/2.
//

#pragma once

#include <core/environment/Singleton.h>
#include <physics/PhysicsBase.h>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::phy {
    class PhysicsWorld;
    class RigidBody;
    class CharacterController;
    class PhysicsShape;

    class PhysicsRegistry : public Singleton<PhysicsRegistry>, public IMeshConfigNotify {
    public:
        PhysicsRegistry() = default;
        ~PhysicsRegistry() override = default;

        static void Reflect(SerializationContext *context);
        void GatherConfigTypes(std::set<Uuid> &typeId) override;

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual PhysicsWorld* CreatePhysicsWorld() = 0;
            virtual RigidBody* CreateRigidBody() = 0;
            virtual CharacterController* CreateCharacterController() = 0;
        };

        PhysicsWorld* CreatePhysicsWorld();
        RigidBody* CreateRigidBody();
        CharacterController* CreateCharacterController();

        void Register(Impl* factory);
        void UnRegister();
    private:
        std::unique_ptr<Impl> factory;
    };

} // namespace sky::phy