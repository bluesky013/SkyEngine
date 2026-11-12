#pragma once

#include <core/math/Transform.h>
#include <core/util/Uuid.h>
#include <physics/PhysicsFilter.h>
#include <physics/PhysicsObjectId.h>
#include <physics/PhysicsStatus.h>

#include <array>
#include <cstdint>
#include <vector>

namespace sky::phy {

    enum class SoftBodyAttachmentMode : uint8_t { Dynamic = 0, WorldFixed, RootKinematic, END };

    struct SoftBodyVertex {
        Vector3                position    = VEC3_ZERO;
        Vector3                velocity    = VEC3_ZERO;
        float                  inverseMass = 1.f;
        SoftBodyAttachmentMode mode        = SoftBodyAttachmentMode::Dynamic;
    };

    struct SoftBodyEdge {
        std::array<uint32_t, 2> vertices{};
        float                   compliance = 0.f;
    };

    struct SoftBodyTetrahedron {
        std::array<uint32_t, 4> vertices{};
        float                   compliance = 0.f;
    };

    struct SoftBodyRootFollow {
        uint32_t vertex     = 0;
        float    compliance = .02f;
    };

    // Rest positions and velocities use metres in root-local space. Face winding is outward.
    struct SoftBodyMesh {
        std::vector<SoftBodyVertex>          vertices;
        std::vector<SoftBodyEdge>            edges;
        std::vector<SoftBodyTetrahedron>     tetrahedra;
        std::vector<std::array<uint32_t, 3>> faces;
    };

    struct SoftBodyDesc {
        SoftBodyMesh mesh;
        // Dynamic vertices follow their root-local rest positions with strictly positive compliance.
        std::vector<SoftBodyRootFollow> rootFollow;
        Transform                       transform;
        CollisionFilter                 filter;
        uint32_t                        solverIterations  = 5;
        float                           pressure          = 0.f;
        float                           gravityFactor     = 1.f;
        float                           linearDamping     = .1f;
        float                           maxLinearVelocity = 500.f;
        float                           vertexRadius      = .01f;
        float                           friction          = .2f;
        float                           restitution       = 0.f;
        bool                            allowSleeping     = true;
    };

    struct SoftBodySnapshot {
        Uuid                 id;
        uint64_t             step     = 0;
        uint64_t             revision = 0;
        std::vector<Vector3> worldPositions;
        std::vector<Vector3> worldVelocities;
    };

    PhysicsStatus ValidateSoftBodyDesc(const SoftBodyDesc &desc);

} // namespace sky::phy
