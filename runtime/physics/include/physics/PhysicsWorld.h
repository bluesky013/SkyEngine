//
// Created by blues on 2024/9/1.
//

#include <physics/RigidBody.h>
#include <physics/CharacterController.h>
#include <framework/world/World.h>
#include <list>
#include <memory>

namespace sky::phy {
    class PhysicsWorld : public IWorldSubSystem {
    public:
        PhysicsWorld() = default;
        ~PhysicsWorld() override = default;

        static constexpr std::string_view NAME = "PhysicsWorld";

        void AddRigidBody(RigidBody *rb);
        void RemoveRigidBody(RigidBody *rb);

        void AddCharacterController(CharacterController *rb);
        void RemoveCharacterController(CharacterController *rb);

        virtual void SetSimulationEnable(bool en) {}
        virtual void SetDebugDrawEnable(bool en) {}

        virtual void SetGravity(const Vector3 &gravity) {}

    protected:
        virtual void AddRigidBodyImpl(RigidBody *rb) = 0;
        virtual void RemoveRigidBodyImpl(RigidBody *rb) = 0;
        virtual void AddCharacterControllerImpl(CharacterController *rb) = 0;
        virtual void RemoveCharacterControllerImpl(CharacterController *rb) = 0;

        std::list<std::unique_ptr<RigidBody>>           rigidBodies;
        std::list<std::unique_ptr<CharacterController>> characterControllers;
    };

} // namespace sky::phy