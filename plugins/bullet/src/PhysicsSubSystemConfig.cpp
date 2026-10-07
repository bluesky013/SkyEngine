//
// Created on 2026/10/07.
//

#include <bullet/PhysicsSubSystemConfig.h>

namespace sky::phy {

    void PhysicsSubSystemConfig::Reflect(SerializationContext *context)
    {
        context->Register<PhysicsSubSystemConfig>("PhysicsSubSystemConfig")
            .Member<&PhysicsSubSystemConfig::gravity>("gravity")
            .Member<&PhysicsSubSystemConfig::fixedTimestep>("fixedTimestep")
            .Member<&PhysicsSubSystemConfig::maxSubSteps>("maxSubSteps")
            .Member<&PhysicsSubSystemConfig::solverIterations>("solverIterations")
            .Member<&PhysicsSubSystemConfig::sleepThreshold>("sleepThreshold")
            .Member<&PhysicsSubSystemConfig::collisionMargin>("collisionMargin")
            .Member<&PhysicsSubSystemConfig::deterministic>("deterministic")
            .Member<&PhysicsSubSystemConfig::continuousCollision>("continuousCollision");
    }

} // namespace sky::phy
