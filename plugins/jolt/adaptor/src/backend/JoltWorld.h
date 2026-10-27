//
// Created on 2026/10/26.
//

#pragma once

// Jolt requires its configuration header before every other SDK header.
#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <physics/IPhysicsBackend.h>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>

namespace sky::phy {

    struct JoltSdk;
    struct JoltExecutor {
        explicit JoltExecutor(uint32_t workers);
        JPH::JobSystemThreadPool jobs;
        std::mutex               updateMutex;
    };

    struct JoltSession {
        std::shared_ptr<const void>                     codeLease;
        std::shared_ptr<JoltSdk>                        sdk;
        std::mutex                                      executorMutex;
        std::map<uint32_t, std::weak_ptr<JoltExecutor>> executors;
        std::shared_ptr<JoltExecutor>                   GetExecutor(uint32_t workers);
    };

    class JoltLayers final : public JPH::BroadPhaseLayerInterface {
    public:
        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return 2;
        }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            return JPH::BroadPhaseLayer(layer == 0 ? 0 : 1);
        }
    };
    class JoltLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter, public JPH::ObjectLayerPairFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override
        {
            return a != 0 || b.GetValue() != 0;
        }
        bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
        {
            return a != 0 || b != 0;
        }
    };

    class JoltWorld final : public IPhysicsWorld, private JPH::ContactListener, private JPH::CharacterContactListener {
    public:
        explicit JoltWorld(std::shared_ptr<JoltSession> session);
        ~JoltWorld() override;
        PhysicsStatus Init(const PhysicsWorldDesc &desc);
        PhysicsStatus GetLastStatus() const override
        {
            return status;
        }
        const PhysicsBackendCaps &GetCaps() const override
        {
            return caps;
        }

        PhysicsStatus  SetOptions(const PhysicsOptions &options) override;
        PhysicsOptions GetOptions() const override
        {
            return options;
        }

        PhysicsMaterialId CreateMaterial(const PhysicsMaterialData &data) override;
        bool              DestroyMaterial(PhysicsMaterialId id) override;
        bool              GetMaterial(PhysicsMaterialId id, PhysicsMaterialData &out) const override;

        PhysicsObjectId CreateBody(const PhysicsBodyDesc &desc) override;
        PhysicsStatus   UpdateBody(PhysicsObjectId id, BodyKind kind, float mass, const ShapeDesc &shape) override;
        bool            DestroyObject(PhysicsObjectId id) override;
        bool            HasObject(PhysicsObjectId id) const override;

        bool GetBodyTransform(PhysicsObjectId id, Transform &out) const override;
        bool SetBodyTransform(PhysicsObjectId id, const Transform &transform) override;
        bool SetKinematicTarget(PhysicsObjectId id, const Transform &target) override;
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

        PhysicsObjectId     CreateCharacter(const CharacterDesc &desc) override;
        bool                DestroyCharacter(PhysicsObjectId id) override;
        CharacterMoveResult MoveCharacter(PhysicsObjectId id, const Vector3 &displacement) override;
        bool                GetCharacterState(PhysicsObjectId id, CharacterState &out) const override;
        bool                SetCharacterTransform(PhysicsObjectId id, const Transform &transform) override;
        bool                SetCharacterCapsule(PhysicsObjectId id, float radius, float height) override;

        PhysicsStatus Step(float fixedDelta) override;

        bool Raycast(const Vector3            &origin,
                     const Vector3            &direction,
                     float                     maxDistance,
                     const PhysicsQueryFilter &filter,
                     PhysicsQueryHit          &out) const override;
        bool Sweep(const ShapeDesc          &shape,
                   const Transform          &from,
                   const Vector3            &direction,
                   float                     maxDistance,
                   const PhysicsQueryFilter &filter,
                   PhysicsQueryHit          &out) const override;
        void Overlap(const ShapeDesc                  &shape,
                     const Transform                  &transform,
                     const PhysicsQueryFilter         &filter,
                     std::vector<PhysicsQueryOverlap> &out) const override;

        void              DrainEvents(std::vector<PhysicsEvent> &out) override;
        void              CollectDebugGeometry(uint32_t categories, PhysicsDebugGeometry &out) const override;
        PhysicsWorldStats GetStats() const override;

        bool CaptureState(PhysicsWorldState &out, PhysicsSnapshotScope scope) const override;
        bool RestoreState(const PhysicsWorldState &state) override;

        using Pair = std::pair<PhysicsObjectId, PhysicsObjectId>;
        enum class Kind { Body, Character, Constraint, Material };
        struct Entry {
            PhysicsObjectId                        id;
            Kind                                   kind = Kind::Body;
            JPH::BodyID                            body;
            JPH::RefConst<JPH::Shape>              shape;
            std::unique_ptr<JPH::CharacterVirtual> character;
            JPH::Ref<JPH::TwoBodyConstraint>       constraint;
            PhysicsBodyDesc                        desc;
            PhysicsMaterialData                    material;
            CharacterDesc                          characterDesc;
            ConstraintDesc                         constraintDesc;
            CollisionFilter                        filter;
            Vector3                                displacement = VEC3_ZERO;
            Transform                              previous;
            Transform                              current;
            std::optional<Transform>               kinematicTarget;
        };
        Entry *Resolve(PhysicsObjectId id) const;
        Entry *FromNative(JPH::BodyID id) const;
        bool   AcceptPair(const JPH::CollisionGroup &a, const JPH::CollisionGroup &b) const;
        bool   Fail(PhysicsError error, const char *message) const;
        bool   Ready() const;

    private:
        class PairFilter final : public JPH::GroupFilter {
        public:
            explicit PairFilter(const JoltWorld &world) : world(world)
            {
            }
            bool CanCollide(const JPH::CollisionGroup &a, const JPH::CollisionGroup &b) const override
            {
                return world.AcceptPair(a, b);
            }

        private:
            const JoltWorld &world;
        };
        using ContactKey = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>;
        struct Contact {
            PhysicsEvent event;
            bool         removed = false;
        };

        Entry                    &Allocate(Kind kind);
        void                      Release(PhysicsObjectId id);
        JPH::RefConst<JPH::Shape> Cook(const ShapeDesc &desc, const Vector3 &scale = VEC3_ONE, uint32_t depth = 0) const;
        bool                      CheckMaterial(const PhysicsMaterialData &material) const;
        void                      ApplyMaterial(Entry &entry);
        void                      EraseContacts(PhysicsObjectId id);
        void                      PublishContacts();
        void RecordContact(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &manifold, JPH::ContactSettings &settings);
        void OnContactAdded(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &m, JPH::ContactSettings &s) override;
        void OnContactPersisted(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &m, JPH::ContactSettings &s) override;
        void OnContactRemoved(const JPH::SubShapeIDPair &pair) override;
        void UpdateCharacters(float delta);
        bool OnCharacterContactValidate(const JPH::CharacterVirtual *character, const JPH::CharacterContact &contact) override;
        void Synchronize();
        void RefreshBody(Entry &entry);

        std::shared_ptr<JoltSession>             session;
        std::shared_ptr<JoltExecutor>            executor;
        JoltLayers                               layers;
        JoltLayerFilter                          layerFilter;
        JPH::Ref<PairFilter>                     pairFilter;
        JPH::TempAllocatorMalloc                 allocator;
        JPH::CharacterVsCharacterCollisionSimple characterCollisions;
        std::unique_ptr<JPH::PhysicsSystem>      native;
        std::vector<std::unique_ptr<Entry>>      slots;
        std::vector<uint32_t>                    generations;
        std::vector<uint32_t>                    freeSlots;
        std::map<uint32_t, PhysicsObjectId>      nativeIds;
        std::map<Pair, uint32_t>                 exclusions;
        std::mutex                               contactMutex;
        std::map<ContactKey, Contact>            contacts;
        std::map<Pair, PhysicsEvent>             previousPairs;
        std::vector<PhysicsEvent>                events;
        PhysicsBackendCaps                       caps;
        PhysicsWorldDesc                         descriptor;
        PhysicsOptions                           options;
        PhysicsWorldStats                        stats;
        mutable PhysicsStatus                    status;
        bool                                     faulted            = false;
        float                                    interpolationAlpha = 0.f;
        uint32_t                                 worldTag           = 0;
    };

} // namespace sky::phy
