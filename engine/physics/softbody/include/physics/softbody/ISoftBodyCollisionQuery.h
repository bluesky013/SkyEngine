#pragma once

#include <physics/PhysicsFilter.h>
#include <physics/PhysicsQuery.h>
#include <physics/PhysicsStatus.h>

#include <cstdint>
#include <vector>

namespace sky::phy {

    struct SoftBodyCollisionQuery {
        Vector3         start     = VEC3_ZERO;
        Vector3         end       = VEC3_ZERO;
        float           radius    = 0.f;
        float           deltaTime = 0.f;
        uint64_t        step      = 0;
        CollisionFilter filter;
    };

    struct SoftBodyCollisionContact {
        PhysicsObjectId object;
        Vector3         point           = VEC3_ZERO;
        Vector3         normal          = VEC3_ZERO;
        Vector3         surfaceVelocity = VEC3_ZERO;
        float           fraction        = 1.f;
        uint64_t        feature         = 0;
        bool            dynamic         = false;
    };

    // Queries use the most recently completed rigid step. Contact planes are in its end pose.
    class ISoftBodyCollisionQuery {
    public:
        virtual ~ISoftBodyCollisionQuery() = default;
        virtual Vector3 GetGravity() const = 0;
        // Return all sweep/penetration contacts; failure preserves out, including unsupported obstacle motion.
        virtual PhysicsStatus QuerySoftBodyContacts(const SoftBodyCollisionQuery &query, std::vector<SoftBodyCollisionContact> &out) const = 0;
    };

} // namespace sky::phy
