#pragma once

#include <physics/softbody/SoftBodyTypes.h>
#include <physics/softbody/SoftBodyWorldTypes.h>

#include <span>

namespace sky::phy {
    class ISoftBodyCollisionQuery;
}

namespace sky::softbody {
    // Owns all simulation state. Host-thread use; failed operations preserve the last complete snapshot.
    class CpuSoftBodySolver {
    public:
        phy::PhysicsStatus Initialize(const phy::SoftBodyDesc &desc, const Uuid &id);
        phy::PhysicsStatus SetRootTarget(const Transform &root);
        phy::PhysicsStatus Reset(const Transform &root);
        const Uuid        &GetId() const
        {
            return snapshot.id;
        }
        phy::PhysicsStatus        ReadParameters(phy::SoftBodyRuntimeParameters &out) const;
        static phy::PhysicsStatus ApplyParameters(std::span<CpuSoftBodySolver *const> bodies, const phy::SoftBodyParameterPatch &patch);
        // One host fixed substep; solverIterations repeats constraints, not time integration.
        phy::PhysicsStatus Step(float delta, const phy::ISoftBodyCollisionQuery &collision, uint64_t step);
        // All bodies commit together. Contact support is discrete vertex/triangle, without CCD or self-collision.
        static phy::PhysicsStatus StepBatch(std::span<CpuSoftBodySolver *const>    bodies,
                                            float                                  delta,
                                            const phy::ISoftBodyCollisionQuery    &collision,
                                            uint64_t                               step,
                                            const phy::SoftBodySimulationSettings &settings,
                                            phy::SoftBodyStepMetrics              *metrics = nullptr);
        phy::SoftBodySnapshot     GetSnapshot() const;

    private:
        phy::SoftBodyDesc     desc;
        phy::SoftBodySnapshot snapshot;
        Transform             rootTarget;
        std::vector<double>   edgeRestLengths;
        std::vector<double>   tetrahedronRestVolumes;
        std::vector<uint8_t>  surfaceVertices;
        bool                  initialized = false;
    };
} // namespace sky::softbody
