//
// Created by blues on 2024/9/1.
//

#include <physics/PhysicsWorld.h>
#include <physics/PhysicsDebugDraw.h>
#include <btBulletCollisionCommon.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <memory>

namespace sky {
    class SerializationContext;
};

namespace sky::phy {

    struct BulletPhysicsConfig {
        Vector3 gravity = Vector3{0.f, -9.8f, 0.f};
    };

    class BulletPhysicsWorld : public PhysicsWorld {
    public:
        BulletPhysicsWorld();
        ~BulletPhysicsWorld() override;

        static void Reflect(SerializationContext *context);
        btDiscreteDynamicsWorld *GetWorld() const { return dynamicWorld.get(); }
    private:
        void Tick(float delta) override;
        void StartSimulation() override;
        void StopSimulation() override;

        void SetDebugDrawEnable(bool en) override;
        void SetGravity(const Vector3 &gravity) override;

        void AddRigidBodyImpl(RigidBody *rb) override;
        void RemoveRigidBodyImpl(RigidBody *rb) override;
        void AddCharacterControllerImpl(CharacterController *rb) override;
        void RemoveCharacterControllerImpl(CharacterController *rb) override;

        std::unique_ptr<btDefaultCollisionConfiguration> configuration;
        std::unique_ptr<btCollisionDispatcher> dispatcher;
        std::unique_ptr<btBroadphaseInterface> broadPhase;
        std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
        std::unique_ptr<btDiscreteDynamicsWorld> dynamicWorld;

        bool enableSimulation = true;
        bool enableDebugDraw  = true;
        std::unique_ptr<PhysicsDebugDraw> debugDraw;
    };

} // namespace sky::phy