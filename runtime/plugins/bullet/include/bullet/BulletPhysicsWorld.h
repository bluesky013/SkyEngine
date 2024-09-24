//
// Created by blues on 2024/9/1.
//

#include <physics/PhysicsWorld.h>
#include <physics/PhysicsDebugDraw.h>
#include <btBulletCollisionCommon.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <memory>

namespace sky::phy {

    class BulletPhysicsWorld : public PhysicsWorld {
    public:
        BulletPhysicsWorld();
        ~BulletPhysicsWorld() override;

    private:
        void ProcessPendingTasks();

        void Tick(float delta) override;
        void SetSimulationEnable(bool en) override;
        void SetDebugDrawEnable(bool en) override;
        void SetGravity(const Vector3 &gravity) override;

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