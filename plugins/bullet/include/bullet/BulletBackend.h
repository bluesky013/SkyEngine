//
// Created on 2026/09/23.
//

#pragma once

#include <physics/IPhysicsBackend.h>

#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace sky::phy {

    class BulletBackendWorld;

    // Fast-mode backend (native float). Bullet is used unmodified; cross-platform determinism is not
    // advertised and Exact mode is rejected by the registry.
    class BulletBackend : public IPhysicsBackend {
    public:
        BulletBackend();
        ~BulletBackend() override = default;

        const PhysicsBackendCaps &GetCaps() const override { return caps; }
        bool Init() override { return true; }
        void Shutdown() override {}

        IPhysicsWorld *CreateWorld(const PhysicsWorldDesc &desc) override;
        void           DestroyWorld(IPhysicsWorld *world) override;

    private:
        PhysicsBackendCaps caps;
    };

    // Bullet user pointer payload. Public so query/debug callbacks can resolve an object to its engine
    // handle and filter without exposing the world's internal entry type.
    struct BulletObjectTag {
        PhysicsObjectId id;
        CollisionFilter filter;
        bool            isTrigger = false;
    };

    class BulletBackendWorld : public IPhysicsWorld {
    public:
        explicit BulletBackendWorld(const PhysicsWorldDesc &desc);
        ~BulletBackendWorld() override;

        const PhysicsBackendCaps &GetCaps() const override { return caps; }

        void           SetOptions(const PhysicsOptions &options) override { this->options = options; }
        PhysicsOptions GetOptions() const override { return options; }

        PhysicsMaterialId CreateMaterial(const PhysicsMaterialData &data) override;
        bool              DestroyMaterial(PhysicsMaterialId id) override;
        bool              GetMaterial(PhysicsMaterialId id, PhysicsMaterialData &out) const override;

        PhysicsObjectId CreateBody(const PhysicsBodyDesc &desc) override;
        bool            DestroyObject(PhysicsObjectId id) override;
        bool            HasObject(PhysicsObjectId id) const override;

        bool GetBodyTransform(PhysicsObjectId id, Transform &out) const override;
        bool SetBodyTransform(PhysicsObjectId id, const Transform &transform) override;
        void SetInterpolationAlpha(float alpha) override;
        bool GetInterpolatedTransform(PhysicsObjectId id, Transform &out) const override;

        bool GetBodyVelocity(PhysicsObjectId id, Vector3 &linear, Vector3 &angular) const override;
        bool SetBodyVelocity(PhysicsObjectId id, const Vector3 &linear, const Vector3 &angular) override;
        bool ApplyForce(PhysicsObjectId id, const Vector3 &force, const Vector3 &atWorldPos) override;
        bool ApplyImpulse(PhysicsObjectId id, const Vector3 &impulse, const Vector3 &atWorldPos) override;

        bool SetBodyFilter(PhysicsObjectId id, const CollisionFilter &filter) override;
        bool SetBodyMaterial(PhysicsObjectId id, PhysicsMaterialId material) override;
        bool Wake(PhysicsObjectId id) override;

        PhysicsObjectId CreateConstraint(const ConstraintDesc &desc) override;
        bool            DestroyConstraint(PhysicsObjectId id) override;

        PhysicsObjectId CreateCharacter(const CharacterDesc &desc) override;
        bool            DestroyCharacter(PhysicsObjectId id) override;
        CharacterMoveResult MoveCharacter(PhysicsObjectId id, const Vector3 &displacement) override;
        bool GetCharacterState(PhysicsObjectId id, CharacterState &out) const override;
        bool SetCharacterTransform(PhysicsObjectId id, const Transform &transform) override;
        bool SetCharacterCapsule(PhysicsObjectId id, float radius, float height) override;

        void Step(float fixedDelta) override;

        bool Raycast(const Vector3 &origin, const Vector3 &direction, float maxDistance,
                     const PhysicsQueryFilter &filter, PhysicsQueryHit &out) const override;
        bool Sweep(const ShapeDesc &shape, const Transform &from, const Vector3 &direction, float maxDistance,
                   const PhysicsQueryFilter &filter, PhysicsQueryHit &out) const override;
        void Overlap(const ShapeDesc &shape, const Transform &transform,
                     const PhysicsQueryFilter &filter, std::vector<PhysicsQueryOverlap> &out) const override;

        void DrainEvents(std::vector<PhysicsEvent> &out) override;
        void CollectDebugGeometry(uint32_t categories, PhysicsDebugGeometry &out) const override;
        PhysicsWorldStats GetStats() const override;

        bool CaptureState(PhysicsWorldState &out, PhysicsSnapshotScope scope) const override;
        bool RestoreState(const PhysicsWorldState &state) override;

        btDiscreteDynamicsWorld *GetBulletWorld() const { return dynamicWorld.get(); }

    private:
        enum class ObjectKind : uint8_t {
            Body = 0,
            Character,
            Constraint
        };

        struct ObjectEntry {
            PhysicsObjectId id;
            ObjectKind      kind = ObjectKind::Body;

            // Owns the Bullet user pointer payload; must outlive the Bullet object it is attached to.
            std::unique_ptr<BulletObjectTag>  tag;
            std::unique_ptr<btCollisionShape> shape;
            std::unique_ptr<btRigidBody>      body;
            std::unique_ptr<btDefaultMotionState> motion;
            std::unique_ptr<btStridingMeshInterface> meshInterface;
            std::vector<float>                heightSamples;

            std::unique_ptr<btPairCachingGhostObject>     ghost;
            std::unique_ptr<btKinematicCharacterController> controller;
            std::unique_ptr<btCapsuleShape>               characterShape;

            std::unique_ptr<btTypedConstraint> constraint;
            PhysicsObjectId                    constraintA = INVALID_PHYSICS_OBJECT_ID;
            PhysicsObjectId                    constraintB = INVALID_PHYSICS_OBJECT_ID;

            CollisionFilter   filter;
            PhysicsMaterialData material;
            bool              hasExplicitMaterial = false;
            bool              isTrigger           = false;

            Transform prev;
            Transform curr;
        };

        struct MaterialEntry {
            PhysicsMaterialData data;
        };

        ObjectEntry *Resolve(PhysicsObjectId id) const;
        PhysicsObjectId AllocateId(ObjectKind kind);
        void Release(uint32_t slot);

        std::unique_ptr<btCollisionShape> CookShape(const ShapeDesc &desc, ObjectEntry &entry) const;
        void ApplyFilter(btCollisionObject &object, const CollisionFilter &filter) const;
        void ApplyMaterial(btRigidBody &body, const PhysicsMaterialData &material) const;
        void SyncTransforms();

        void GatherContactPairs(std::vector<std::pair<PhysicsObjectId, PhysicsObjectId>> &pairs) const;

        std::unique_ptr<btDefaultCollisionConfiguration> configuration;
        std::unique_ptr<btCollisionDispatcher>           dispatcher;
        std::unique_ptr<btBroadphaseInterface>           broadPhase;
        std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
        std::unique_ptr<btDiscreteDynamicsWorld>         dynamicWorld;

        // Slots are never moved so entry pointers stay stable and can be used as Bullet user pointers.
        std::vector<std::unique_ptr<ObjectEntry>>          slots;
        std::vector<uint32_t>                              generations;
        std::vector<uint32_t>                              freeSlots;
        std::vector<std::unique_ptr<MaterialEntry>>        materials;

        // Previous contact/trigger pairs, for begin/stay/end diffing.
        std::set<std::pair<PhysicsObjectId, PhysicsObjectId>> previousPairs;
        std::vector<PhysicsEvent>                             events;

        PhysicsBackendCaps caps;
        PhysicsOptions     options;
        PhysicsWorldStats  stats;
        float              interpolationAlpha = 0.f;
    };

} // namespace sky::phy
