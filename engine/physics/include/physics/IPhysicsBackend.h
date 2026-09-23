//
// Created on 2026/09/23.
//

#pragma once

#include <physics/PhysicsBody.h>
#include <physics/PhysicsCharacter.h>
#include <physics/PhysicsConstraint.h>
#include <physics/PhysicsDeterminism.h>
#include <physics/PhysicsDebugGeometry.h>
#include <physics/PhysicsEvents.h>
#include <physics/PhysicsFilter.h>
#include <physics/PhysicsMaterial.h>
#include <physics/PhysicsObjectId.h>
#include <physics/PhysicsQuery.h>
#include <physics/PhysicsSnapshot.h>
#include <physics/PhysicsWorldStats.h>

#include <core/math/Transform.h>
#include <core/math/Vector3.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky::phy {

    // Canonical world sub-system name. Every physics backend registers its sub-system under this name so
    // consumers resolve physics without depending on a specific backend plugin.
    inline constexpr std::string_view PHYSICS_SYSTEM_NAME = "Physics";

    // Backend capability descriptor, queried before world creation.
    struct PhysicsBackendCaps {
        PhysicsMathMode mathMode = PhysicsMathMode::Fast;
        bool            deterministic = false;

        bool jobStepping   = false;
        bool supportsCCD   = false;
        bool supportsConstraints  = false;
        bool supportsCharacters   = false;

        bool supportsBox          = true;
        bool supportsSphere       = true;
        bool supportsCapsule      = true;
        bool supportsHeightField  = true;
        bool supportsTriangleMesh = true;
        bool supportsConvexHull   = true;
        bool supportsCompound     = true;
    };

    // Runtime-configurable behavior switches; exposed instead of hard validation so projects/targets can
    // tune physics per world (mobile budgets, determinism experiments, authoring exceptions).
    struct PhysicsOptions {
        bool     allowDynamicTriangleMesh = false; // dynamic bodies may use triangle-mesh collision
        bool     enableSleep              = true;  // bodies may sleep
        bool     enableCCD                = false; // global gate for per-body CCD requests
        uint32_t maxBodies                = 0;     // 0 = unlimited

        // Active-body budget hint; 0 = unlimited.
        uint32_t maxActiveBodies = 0;
    };

    struct PhysicsWorldDesc {
        PhysicsMathMode mathMode = PhysicsMathMode::Fast;
        Vector3         gravity  = Vector3(0.f, -9.81f, 0.f);
        PhysicsOptions  options;
    };

    // Runtime world contract. The world owns every object created through it; callers reference objects
    // by PhysicsObjectId and must not retain backend pointers.
    class IPhysicsWorld {
    public:
        IPhysicsWorld() = default;
        virtual ~IPhysicsWorld() = default;

        virtual         const PhysicsBackendCaps &GetCaps() const = 0;

        virtual void           SetOptions(const PhysicsOptions &options) = 0;
        virtual PhysicsOptions GetOptions() const = 0;

        // Materials
        virtual PhysicsMaterialId CreateMaterial(const PhysicsMaterialData &data) = 0;
        virtual bool              DestroyMaterial(PhysicsMaterialId id) = 0;
        virtual bool              GetMaterial(PhysicsMaterialId id, PhysicsMaterialData &out) const = 0;

        // Bodies
        virtual PhysicsObjectId CreateBody(const PhysicsBodyDesc &desc) = 0;
        virtual bool            DestroyObject(PhysicsObjectId id) = 0;
        virtual bool            HasObject(PhysicsObjectId id) const = 0;

        virtual bool GetBodyTransform(PhysicsObjectId id, Transform &out) const = 0;
        virtual bool SetBodyTransform(PhysicsObjectId id, const Transform &transform) = 0;

        // Alpha in [0,1) between the previous and current fixed step; used to blend rendered poses.
        virtual void SetInterpolationAlpha(float alpha) = 0;
        virtual bool GetInterpolatedTransform(PhysicsObjectId id, Transform &out) const = 0;

        virtual bool GetBodyVelocity(PhysicsObjectId id, Vector3 &linear, Vector3 &angular) const = 0;
        virtual bool SetBodyVelocity(PhysicsObjectId id, const Vector3 &linear, const Vector3 &angular) = 0;
        virtual bool ApplyForce(PhysicsObjectId id, const Vector3 &force, const Vector3 &atWorldPos) = 0;
        virtual bool ApplyImpulse(PhysicsObjectId id, const Vector3 &impulse, const Vector3 &atWorldPos) = 0;

        virtual bool SetBodyFilter(PhysicsObjectId id, const CollisionFilter &filter) = 0;
        virtual bool SetBodyMaterial(PhysicsObjectId id, PhysicsMaterialId material) = 0;
        virtual bool Wake(PhysicsObjectId id) = 0;

        // Constraints
        virtual PhysicsObjectId CreateConstraint(const ConstraintDesc &desc) = 0;
        virtual bool            DestroyConstraint(PhysicsObjectId id) = 0;

        // Characters
        virtual PhysicsObjectId CreateCharacter(const CharacterDesc &desc) = 0;
        virtual bool            DestroyCharacter(PhysicsObjectId id) = 0;
        virtual CharacterMoveResult MoveCharacter(PhysicsObjectId id, const Vector3 &displacement) = 0;
        virtual bool GetCharacterState(PhysicsObjectId id, CharacterState &out) const = 0;
        virtual bool SetCharacterTransform(PhysicsObjectId id, const Transform &transform) = 0;
        virtual bool SetCharacterCapsule(PhysicsObjectId id, float radius, float height) = 0;

        // Stepping
        virtual void Step(float fixedDelta) = 0;

        // Queries
        virtual bool Raycast(const Vector3 &origin, const Vector3 &direction, float maxDistance,
                             const PhysicsQueryFilter &filter, PhysicsQueryHit &out) const = 0;
        virtual bool Sweep(const ShapeDesc &shape, const Transform &from, const Vector3 &direction, float maxDistance,
                           const PhysicsQueryFilter &filter, PhysicsQueryHit &out) const = 0;
        virtual void Overlap(const ShapeDesc &shape, const Transform &transform,
                             const PhysicsQueryFilter &filter, std::vector<PhysicsQueryOverlap> &out) const = 0;

        // Events, debug, stats, snapshot
        virtual void DrainEvents(std::vector<PhysicsEvent> &out) = 0;
        virtual void CollectDebugGeometry(uint32_t categories, PhysicsDebugGeometry &out) const = 0;
        virtual PhysicsWorldStats GetStats() const = 0;

        virtual bool CaptureState(PhysicsWorldState &out, PhysicsSnapshotScope scope = PhysicsSnapshotScope::DynamicKinematic) const = 0;
        virtual bool RestoreState(const PhysicsWorldState &state) = 0;
    };

    // Backend contract. Exactly one backend is active at a time and is registered from its module Start.
    class IPhysicsBackend {
    public:
        IPhysicsBackend() = default;
        virtual ~IPhysicsBackend() = default;

        virtual const PhysicsBackendCaps &GetCaps() const = 0;

        virtual bool Init() = 0;
        virtual void Shutdown() = 0;

        virtual IPhysicsWorld *CreateWorld(const PhysicsWorldDesc &desc) = 0;
        virtual void           DestroyWorld(IPhysicsWorld *world) = 0;
    };

} // namespace sky::phy
