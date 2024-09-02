//
// Created by blues on 2024/9/1.
//

#include <physics/RigidBody.h>
#include <physics/CharacterController.h>
#include <framework/world/World.h>
#include <list>
#include <memory>

namespace sky::phy {

    enum class BuildOperation : uint32_t {
        BUILD,
        DESTROY
    };

    enum class PhysicsObjectType : uint32_t {
        RIGIDBODY,
        CHARACTER_CONTROLLER,
    };

    struct PhysicsObjectTask {
        BuildOperation      op;
        PhysicsObjectType   type;
        union {
            RigidBody           *rigidBody;
            CharacterController *character;
        };

        explicit PhysicsObjectTask(RigidBody* rb, BuildOperation operation)
                : op(operation)
                , type(PhysicsObjectType::RIGIDBODY)
        {
            rigidBody = rb;
        }

        explicit PhysicsObjectTask(CharacterController* ch, BuildOperation operation)
                : op(operation)
                , type(PhysicsObjectType::CHARACTER_CONTROLLER)
        {
            character = ch;
        }

    };

    class PhysicsWorld : public IWorldSubSystem {
    public:
        PhysicsWorld() = default;
        ~PhysicsWorld() override = default;

        void StartSimulation();
        void StopSimulation();

        virtual void Tick(float delta) = 0;

        void AddRigidBody(RigidBody *rb);
        void RemoveRigidBody(RigidBody *rb);

        void AddCharacterController(CharacterController *rb);
        void RemoveCharacterController(CharacterController *rb);

        virtual void SetDebugDrawEnable(bool en) {}

    protected:
        virtual void StartImpl() {}
        virtual void StopImpl() {}

        void AddRigidBodyOperation(const PhysicsObjectTask &task);

        bool enableSimulation = false;

        std::list<std::unique_ptr<RigidBody>>           rigidBodies;
        std::list<std::unique_ptr<CharacterController>> characterControllers;

        std::vector<PhysicsObjectTask> pendingTasks;
    };

} // namespace sky::phy