#pragma once

#include <core/util/Uuid.h>
#include <physics/softbody/SoftBodyWorldTypes.h>
#include <span>
#include <vector>

namespace sky::phy {
    class ISoftBodyWorldControl {
    public:
        virtual ~ISoftBodyWorldControl() = default;
        // Host-thread only. Runtime UUIDs identify live World sessions, never persistent asset IDs.
        virtual PhysicsStatus Configure(const Uuid &world, const SoftBodySimulationSettings &settings) = 0;
        // bodyCount reports currently ready bodies; other fields describe the last attempted batch.
        virtual PhysicsStatus GetStats(const Uuid &world, SoftBodyStepMetrics &out) const = 0;
        // Returns owned samples newer than afterStep. Expired cursors fail without changing out.
        virtual PhysicsStatus ReadSamples(const Uuid &world, uint64_t afterStep, std::vector<SoftBodyStepMetrics> &out) const = 0;
        // Host-thread read of owned presentation/committed particles and active rig topology. Failure preserves out.
        virtual PhysicsStatus ReadDebugBodies(const Uuid &world, std::vector<SoftBodyDebugBody> &out) const            = 0;
        virtual PhysicsStatus ReadBodyParameters(const Uuid &world, std::vector<SoftBodyRuntimeParameters> &out) const = 0;
        // Host-thread only, between fixed steps. All simulation instance IDs must still be live; failure changes none.
        virtual PhysicsStatus ApplyBodyParameters(const Uuid &world, std::span<const Uuid> bodyIds, const SoftBodyParameterPatch &patch) = 0;
    };
} // namespace sky::phy
