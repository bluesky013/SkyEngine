//
// Created by blues on 2024/9/1.
//

#include <physics/PhysicsWorld.h>

namespace sky::phy {

    void PhysicsWorld::AddRigidBody(RigidBody *rb)
    {
        AddRigidBodyOperation(PhysicsObjectTask(rb, BuildOperation::BUILD));
        rigidBodies.emplace_back(rb);
    }

    void PhysicsWorld::RemoveRigidBody(RigidBody *rb)
    {
        auto iter = std::find_if(rigidBodies.begin(), rigidBodies.end(), [rb](const auto &val) {
            return rb == val.get();
        });
        if (iter != rigidBodies.end()) {
            AddRigidBodyOperation(PhysicsObjectTask(rb, BuildOperation::DESTROY));
            rigidBodies.erase(iter);
        }
    }

    void PhysicsWorld::AddCharacterController(CharacterController *ch)
    {
        AddRigidBodyOperation(PhysicsObjectTask(ch, BuildOperation::BUILD));
        characterControllers.emplace_back(ch);
    }

    void PhysicsWorld::RemoveCharacterController(CharacterController *ch)
    {
        auto iter = std::find_if(characterControllers.begin(), characterControllers.end(), [ch](const auto &val) {
            return ch == val.get();
        });
        if (iter != characterControllers.end()) {
            AddRigidBodyOperation(PhysicsObjectTask(ch, BuildOperation::DESTROY));
            characterControllers.erase(iter);
        }
    }

    void PhysicsWorld::AddRigidBodyOperation(const PhysicsObjectTask &task)
    {
        pendingTasks.emplace_back(task);
    }
} // namespace sky::phy