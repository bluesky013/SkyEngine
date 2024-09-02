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

        void Tick(float delta) override;

        void SetDebugDrawEnable(bool en) override;

    private:
        void ProcessPendingTasks();

        std::unique_ptr<btDefaultCollisionConfiguration> configuration;
        std::unique_ptr<btCollisionDispatcher> dispatcher;
        std::unique_ptr<btBroadphaseInterface> broadPhase;
        std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
        std::unique_ptr<btDiscreteDynamicsWorld> dynamicWorld;

        bool enableDebugDraw = false;
        std::unique_ptr<PhysicsDebugDraw> debugDraw;
    };

} // namespace sky::phy