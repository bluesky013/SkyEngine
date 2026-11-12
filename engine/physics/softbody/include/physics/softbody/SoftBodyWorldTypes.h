#pragma once

#include <core/math/Vector3.h>
#include <core/util/Uuid.h>
#include <physics/PhysicsStatus.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace sky::phy {
    struct SoftBodyComplianceRange {
        size_t count   = 0;
        float  minimum = 0.f, maximum = 0.f;
    };

    struct SoftBodyRuntimeParameters {
        Uuid                    id, actor;
        uint32_t                controlPoints = 0, solverIterations = 0;
        float                   pressure = 0.f, linearDamping = 0.f;
        SoftBodyComplianceRange followCompliance, edgeCompliance, volumeCompliance;
    };

    // A supplied compliance replaces every existing constraint of that kind on each selected body.
    struct SoftBodyParameterPatch {
        std::optional<float> followCompliance, edgeCompliance, volumeCompliance, linearDamping;
    };

    struct SoftBodySimulationSettings {
        bool  rigidCollision        = true;
        bool  softCollision         = false;
        bool  profileSteps          = false;
        float softContactCompliance = 0.f;
    };

    struct SoftBodyDebugBody {
        Uuid     id, actor;
        uint64_t step = 0, revision = 0;
        uint64_t presentationStep = 0, presentationRevision = 0;
        float    interpolationAlpha = 1.f, vertexRadius = 0.f;
        // Last presented positions are the same interpolated particles consumed by the animation pose contributor.
        std::vector<Vector3> worldPositions;
        // Last successfully published fixed-step state; it may be newer than presentationStep.
        std::vector<Vector3>                 committedWorldPositions;
        std::vector<std::array<uint32_t, 2>> edges;
        std::vector<std::array<uint32_t, 3>> faces;
    };

    // Counts describe one attempted fixed step, including work before a failed transaction.
    struct SoftBodyStepMetrics {
        uint64_t     step           = 0;
        uint64_t     attemptedSteps = 0;
        uint64_t     completedSteps = 0;
        uint64_t     bodyCount      = 0;
        bool         committed      = false;
        PhysicsError failure        = PhysicsError::None;
        uint64_t     particles = 0, surfaceVertices = 0, edges = 0, tetrahedra = 0, faces = 0;
        uint64_t     internalIterations = 0;
        uint64_t     broadphasePairs = 0, broadphaseAccepted = 0;
        uint64_t     vertexTriangleTests = 0, tetContainmentTests = 0, segmentTriangleTests = 0;
        uint64_t     softContacts = 0, activeSoftContacts = 0;
        uint64_t     rigidQueries = 0, rigidContacts = 0, activeRigidContacts = 0;
        double       maxPenetration = 0.;
        uint64_t     predictNs = 0, internalXpbdNs = 0;
        uint64_t     rigidQueryNs = 0, rigidProjectionNs = 0;
        uint64_t     softBroadphaseNs = 0, softNarrowphaseNs = 0, softProjectionNs = 0;
        uint64_t     finalizeNs = 0, totalNs = 0;
    };
} // namespace sky::phy
