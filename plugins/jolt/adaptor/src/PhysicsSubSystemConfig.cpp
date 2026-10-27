//
// Created on 2026/10/07.
//

#include <jolt/PhysicsSubSystemConfig.h>

#include <cmath>

namespace sky::phy {
    PhysicsStatus PhysicsSubSystemConfig::MakeSettings(PhysicsWorldDesc &world, PhysicsStepConfig &step) const
    {
        if (deterministic) {
            return {PhysicsError::Unsupported, "Deterministic physics is not supported by this Jolt configuration"};
        }
        if (!std::isfinite(gravity) || !std::isfinite(fixedTimestep) || fixedTimestep <= 0 || maxSubSteps <= 0 || solverIterations <= 0 ||
            solverIterations > 255 || !std::isfinite(sleepThreshold) || sleepThreshold < 0 || !std::isfinite(collisionMargin) ||
            collisionMargin < 0 || nativeBodyCapacity == 0 || nativeBodyCapacity > 0x7fffff || nativeBodyPairCapacity == 0 ||
            nativeContactCapacity == 0 || workerCount > 64) {
            return {PhysicsError::InvalidArgument, "Invalid physics timestep, capacity or solver configuration"};
        }
        world                   = {};
        world.gravity           = {0, gravity, 0};
        world.solverIterations  = static_cast<uint32_t>(solverIterations);
        world.sleepThreshold    = sleepThreshold;
        world.collisionMargin   = collisionMargin;
        world.options.enableCCD = continuousCollision;
        world.limits            = {nativeBodyCapacity, characterCapacity, nativeBodyPairCapacity, nativeContactCapacity};
        world.workerCount       = workerCount;
        step                    = {fixedTimestep, static_cast<uint32_t>(maxSubSteps)};
        return {};
    }

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
            .Member<&PhysicsSubSystemConfig::continuousCollision>("continuousCollision")
            .Member<&PhysicsSubSystemConfig::nativeBodyCapacity>("nativeBodyCapacity")
            .Member<&PhysicsSubSystemConfig::nativeBodyPairCapacity>("nativeBodyPairCapacity")
            .Member<&PhysicsSubSystemConfig::nativeContactCapacity>("nativeContactCapacity")
            .Member<&PhysicsSubSystemConfig::characterCapacity>("characterCapacity")
            .Member<&PhysicsSubSystemConfig::workerCount>("workerCount");
    }

} // namespace sky::phy
