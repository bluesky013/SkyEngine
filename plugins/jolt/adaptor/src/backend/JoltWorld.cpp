//
// Created on 2026/10/26.
//

#include "JoltWorld.h"
#include "JoltConversion.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Constraints/ContactConstraintManager.h>
#include <core/environment/Singleton.h>

#include <algorithm>
#include <atomic>
#include <limits>

namespace sky::phy {
    class JoltHandleSpace final : public Singleton<JoltHandleSpace> {
    public:
        std::atomic<uint32_t> nextWorld{1};
    };

    JoltWorld::JoltWorld(std::shared_ptr<JoltSession> value) : session(std::move(value)), pairFilter(new PairFilter(*this))
    {
        worldTag         = JoltHandleSpace::Get()->nextWorld.fetch_add(1);
        caps.jobStepping = caps.supportsCCD = caps.supportsConstraints = caps.supportsCharacters = true;
    }

    JoltWorld::~JoltWorld()
    {
        if (!native) {
            return;
        }
        for (auto &entry : slots) {
            if (entry && entry->kind == Kind::Constraint) {
                native->RemoveConstraint(entry->constraint);
                entry.reset();
            }
        }
        for (auto &entry : slots) {
            if (!entry) {
                continue;
            }
            if (entry->character) {
                characterCollisions.Remove(entry->character.get());
                entry->character.reset();
            } else if (!entry->body.IsInvalid()) {
                if (entry->added)
                    native->GetBodyInterface().RemoveBody(entry->body);
                native->GetBodyInterface().DestroyBody(entry->body);
            }
        }
        slots.clear();
        native.reset();
    }

    bool JoltWorld::Fail(PhysicsError error, const char *message) const
    {
        status = {error, message};
        return false;
    }
    bool JoltWorld::Ready() const
    {
        if (faulted) {
            return false;
        }
        if (!native) {
            return Fail(PhysicsError::InvalidState, "Physics world is not ready");
        }
        status = {};
        return true;
    }

    PhysicsStatus JoltWorld::Init(const PhysicsWorldDesc &desc)
    {
        if (desc.mathMode != PhysicsMathMode::Fast) {
            Fail(PhysicsError::Unsupported, "Jolt supports Fast math only");
            return status;
        }
        if (!Finite(desc.gravity) || desc.limits.bodies == 0 || desc.limits.bodies > JPH::BodyID::cMaxBodyIndex || desc.limits.bodyPairs == 0 ||
            desc.limits.bodyPairs > JPH::ContactConstraintManager::cMaxBodyPairsLimit || desc.limits.contactConstraints == 0 ||
            desc.limits.contactConstraints > JPH::ContactConstraintManager::cMaxContactConstraintsLimit || desc.solverIterations == 0 ||
            desc.solverIterations > 255 || !Finite(desc.sleepThreshold) || desc.sleepThreshold < 0 || !Finite(desc.collisionMargin) ||
            desc.collisionMargin < 0 || desc.workerCount > 64) {
            Fail(PhysicsError::InvalidArgument, "Invalid world limits or solver settings");
            return status;
        }
        if (desc.options.allowDynamicTriangleMesh) {
            Fail(PhysicsError::Unsupported, "Dynamic concave meshes are unsupported");
            return status;
        }
        descriptor = desc;
        options    = desc.options;
        executor   = session->GetExecutor(desc.workerCount);
        native     = std::make_unique<JPH::PhysicsSystem>();
        native->Init(desc.limits.bodies, 0, desc.limits.bodyPairs, desc.limits.contactConstraints, layers, layerFilter, layerFilter);
        native->SetGravity(ToJolt(desc.gravity));
        auto settings                         = native->GetPhysicsSettings();
        settings.mNumVelocitySteps            = desc.solverIterations;
        settings.mNumPositionSteps            = desc.solverIterations;
        settings.mPointVelocitySleepThreshold = desc.sleepThreshold;
        native->SetPhysicsSettings(settings);
        native->SetContactListener(this);
        return status = {};
    }

    JoltWorld::Entry &JoltWorld::Allocate(Kind kind)
    {
        uint32_t index;
        if (freeSlots.empty()) {
            index = static_cast<uint32_t>(slots.size());
            slots.emplace_back();
            generations.push_back(1);
        } else {
            index = freeSlots.back();
            freeSlots.pop_back();
        }
        slots[index] = std::make_unique<Entry>();
        auto &entry  = *slots[index];
        entry.kind   = kind;
        entry.id     = {(uint64_t(worldTag) << 32) | (uint64_t(index) + 1), generations[index]};
        return entry;
    }

    JoltWorld::Entry *JoltWorld::Resolve(PhysicsObjectId id) const
    {
        uint32_t index = uint32_t(id.index);
        if (id.index >> 32 != worldTag || index == 0 || index > slots.size()) {
            return nullptr;
        }
        auto *entry = slots[index - 1].get();
        return entry && entry->id == id ? entry : nullptr;
    }

    JoltWorld::Entry *JoltWorld::FromNative(JPH::BodyID id) const
    {
        auto found = nativeIds.find(id.GetIndexAndSequenceNumber());
        return found != nativeIds.end() ? Resolve(found->second) : nullptr;
    }

    void JoltWorld::Release(PhysicsObjectId id)
    {
        uint32_t index = uint32_t(id.index) - 1;
        slots[index].reset();
        if (++generations[index] != 0) {
            freeSlots.push_back(index);
        }
    }

    bool JoltWorld::AcceptPair(const JPH::CollisionGroup &a, const JPH::CollisionGroup &b) const
    {
        auto *first  = Resolve({(uint64_t(worldTag) << 32) | a.GetGroupID(), a.GetSubGroupID()});
        auto *second = Resolve({(uint64_t(worldTag) << 32) | b.GetGroupID(), b.GetSubGroupID()});
        if (!first || !second || !FilterAccepts(first->filter, second->filter)) {
            return false;
        }
        return exclusions.find(std::minmax(first->id, second->id)) == exclusions.end();
    }

    bool JoltWorld::CheckMaterial(const PhysicsMaterialData &m) const
    {
        if (!Finite(m.staticFriction) || !Finite(m.dynamicFriction) || m.staticFriction < 0 || m.dynamicFriction < 0 || !Finite(m.restitution) ||
            m.restitution < 0 || m.restitution > 1 || !Finite(m.linearDamping) || m.linearDamping < 0 || !Finite(m.angularDamping) ||
            m.angularDamping < 0 || m.frictionCombine > PhysicsCombineMode::Max || m.restitutionCombine > PhysicsCombineMode::Max) {
            return Fail(PhysicsError::InvalidArgument, "Invalid physics material");
        }
        return m.staticFriction == m.dynamicFriction || Fail(PhysicsError::Unsupported, "Separate static and dynamic friction are unsupported");
    }

    PhysicsMaterialId JoltWorld::CreateMaterial(const PhysicsMaterialData &data)
    {
        if (!Ready() || !CheckMaterial(data)) {
            return {};
        }
        auto &entry    = Allocate(Kind::Material);
        entry.material = data;
        return entry.id;
    }
    bool JoltWorld::GetMaterial(PhysicsMaterialId id, PhysicsMaterialData &out) const
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Material) {
            return Fail(PhysicsError::InvalidArgument, "Invalid material handle");
        }
        out = entry->material;
        return true;
    }
    bool JoltWorld::DestroyMaterial(PhysicsMaterialId id)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Material) {
            return Fail(PhysicsError::InvalidArgument, "Invalid material handle");
        }
        for (const auto &body : slots) {
            if (body && body->kind == Kind::Body && body->desc.material == id) {
                return Fail(PhysicsError::InvalidState, "Material is still in use");
            }
        }
        Release(id);
        return true;
    }

    static bool Isotropic(const Vector3 &v)
    {
        return Finite(v) && v.x >= 0 && v.x == v.y && v.x == v.z;
    }
    static bool HasConcave(const ShapeDesc &s)
    {
        if (s.type == ShapeType::TriangleMesh || s.type == ShapeType::HeightField) {
            return true;
        }
        return s.type == ShapeType::Compound && std::any_of(s.children.begin(), s.children.end(), HasConcave);
    }
    PhysicsObjectId JoltWorld::CreateBody(const PhysicsBodyDesc &desc)
    {
        if (!Ready()) {
            return {};
        }
        if (!ValidTransform(desc.transform) || desc.kind > BodyKind::Kinematic || !Finite(desc.mass) || desc.mass <= 0 || !Finite(desc.inertia) ||
            desc.inertia.x < 0 || desc.inertia.y < 0 || desc.inertia.z < 0) {
            Fail(PhysicsError::InvalidArgument, "Invalid body descriptor");
            return {};
        }
        if (!Isotropic(desc.linearDamping) || !Isotropic(desc.angularDamping)) {
            Fail(PhysicsError::Unsupported, "Damping must be finite, nonnegative and isotropic");
            return {};
        }
        if (native->GetNumBodies() >= descriptor.limits.bodies || (options.maxBodies && stats.bodyCount >= options.maxBodies)) {
            Fail(PhysicsError::CapacityExceeded, "Body capacity exceeded");
            return {};
        }
        if (options.maxActiveBodies && desc.kind != BodyKind::Static && !desc.startsAsleep &&
            GetStats().activeBodyCount + std::count_if(staged.begin(), staged.end(),
                                                       [this](auto id) {
                                                           const auto *body = Resolve(id);
                                                           return body && body->desc.kind != BodyKind::Static && !body->desc.startsAsleep;
                                                       }) >=
                options.maxActiveBodies) {
            Fail(PhysicsError::CapacityExceeded, "Active body budget exceeded");
            return {};
        }
        PhysicsMaterialData material;
        if (desc.material != INVALID_PHYSICS_OBJECT_ID && !GetMaterial(desc.material, material)) {
            return {};
        }
        JPH::RefConst<JPH::Shape> shape;
        const ShapeDesc          *geometry = &desc.shape;
        if (desc.preparedShape) {
            if (desc.preparedShape->GetOwner() != session.get()) {
                Fail(PhysicsError::InvalidArgument, "Prepared shape belongs to another backend session");
                return {};
            }
            const auto &prepared = static_cast<const JoltPreparedShape &>(*desc.preparedShape);
            if (prepared.margin != descriptor.collisionMargin || prepared.scale.x != desc.transform.scale.x ||
                prepared.scale.y != desc.transform.scale.y || prepared.scale.z != desc.transform.scale.z) {
                Fail(PhysicsError::InvalidArgument, "Prepared shape options differ from body options");
                return {};
            }
            shape    = prepared.native;
            geometry = prepared.source.get();
        } else {
            shape = Cook(desc.shape, desc.transform.scale);
        }
        if (!shape) {
            return {};
        }
        if (desc.kind != BodyKind::Static && HasConcave(*geometry)) {
            Fail(PhysicsError::Unsupported, "Concave shapes require a static body");
            return {};
        }
        auto &entry = Allocate(Kind::Body);
        entry.shape = shape;
        entry.desc  = desc;
        if (desc.preparedShape)
            entry.desc.shape = *geometry;
        entry.filter   = desc.filter;
        entry.material = material;
        entry.previous = entry.current   = desc.transform;
        auto                      motion = desc.kind == BodyKind::Static    ? JPH::EMotionType::Static
                                           : desc.kind == BodyKind::Dynamic ? JPH::EMotionType::Dynamic
                                                                            : JPH::EMotionType::Kinematic;
        JPH::BodyCreationSettings settings(shape, ToJolt(desc.transform.translation), ToJolt(desc.transform.rotation), motion,
                                           desc.kind == BodyKind::Static && !desc.isTrigger ? 0 : 1);
        settings.mUserData                     = entry.id.index;
        settings.mCollisionGroup               = JPH::CollisionGroup(pairFilter, uint32_t(entry.id.index), entry.id.generation);
        settings.mIsSensor                     = desc.isTrigger;
        settings.mAllowDynamicOrKinematic      = !HasConcave(*geometry);
        settings.mCollideKinematicVsNonDynamic = desc.isTrigger;
        settings.mMotionQuality =
            options.enableCCD && desc.enableCCD && desc.kind == BodyKind::Dynamic ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
        settings.mGravityFactor  = desc.gravityEnabled ? 1.f : 0.f;
        settings.mAllowSleeping  = options.enableSleep && desc.allowSleep;
        settings.mFriction       = material.dynamicFriction;
        settings.mRestitution    = material.restitution;
        settings.mLinearDamping  = desc.linearDamping.x != 0 ? desc.linearDamping.x : material.linearDamping;
        settings.mAngularDamping = desc.angularDamping.x != 0 ? desc.angularDamping.x : material.angularDamping;
        if (settings.mAllowDynamicOrKinematic || desc.kind != BodyKind::Static) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            settings.mMassPropertiesOverride = shape->GetMassProperties();
            settings.mMassPropertiesOverride.ScaleToMass(desc.mass);
            for (uint32_t axis = 0; axis < 3; ++axis) {
                if (desc.inertia[axis] > 0) {
                    settings.mMassPropertiesOverride.mInertia(axis, axis) = desc.inertia[axis];
                }
            }
        }
        auto *body = native->GetBodyInterface().CreateBody(settings);
        if (!body) {
            auto id = entry.id;
            Release(id);
            Fail(PhysicsError::CapacityExceeded, "Jolt body allocation failed");
            return {};
        }
        entry.body                                        = body->GetID();
        nativeIds[entry.body.GetIndexAndSequenceNumber()] = entry.id;
        if (activation) {
            staged.push_back(entry.id);
        } else {
            native->GetBodyInterface().AddBody(entry.body, desc.startsAsleep || desc.kind == BodyKind::Static ? JPH::EActivation::DontActivate
                                                                                                              : JPH::EActivation::Activate);
            entry.added = true;
        }
        ++stats.bodyCount;
        return entry.id;
    }

    PhysicsStatus
    JoltWorld::UpdateBody(PhysicsObjectId id, BodyKind kind, float mass, const ShapeDesc &description, std::shared_ptr<const IPhysicsShape> prepared)
    {
        if (!Ready())
            return status;
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body || kind > BodyKind::Kinematic || !Finite(mass) || mass <= 0) {
            Fail(PhysicsError::InvalidArgument, "Invalid body properties or handle");
            return status;
        }
        if (prepared && prepared->GetOwner() != session.get()) {
            Fail(PhysicsError::InvalidArgument, "Prepared shape belongs to another session");
            return status;
        }
        const auto *cooked   = prepared ? static_cast<const JoltPreparedShape *>(prepared.get()) : nullptr;
        const auto &geometry = cooked ? *cooked->source : description;
        if (cooked && (cooked->margin != descriptor.collisionMargin || cooked->scale.x != entry->current.scale.x ||
                       cooked->scale.y != entry->current.scale.y || cooked->scale.z != entry->current.scale.z)) {
            Fail(PhysicsError::InvalidArgument, "Prepared shape options differ from body options");
            return status;
        }
        if (kind != BodyKind::Static && HasConcave(geometry)) {
            Fail(PhysicsError::Unsupported, "Concave shapes require a static body");
            return status;
        }
        auto shape = cooked ? cooked->native : Cook(description, entry->current.scale);
        if (!shape)
            return status;
        std::vector<Entry *> connected;
        for (const auto &candidate : slots) {
            if (!candidate || candidate->kind != Kind::Constraint || (candidate->constraintDesc.bodyA != id && candidate->constraintDesc.bodyB != id))
                continue;
            const auto  otherId = candidate->constraintDesc.bodyA == id ? candidate->constraintDesc.bodyB : candidate->constraintDesc.bodyA;
            const auto *other   = Resolve(otherId);
            if (kind == BodyKind::Static && (!other || other->desc.kind == BodyKind::Static)) {
                Fail(PhysicsError::Unsupported, "A constrained pair must retain a non-static body");
                return status;
            }
            connected.push_back(candidate.get());
        }
        if (options.maxActiveBodies && kind != BodyKind::Static && !native->GetBodyInterface().IsActive(entry->body) &&
            GetStats().activeBodyCount >= options.maxActiveBodies) {
            Fail(PhysicsError::CapacityExceeded, "Active body budget exceeded");
            return status;
        }
        JPH::BodyLockWrite lock(native->GetBodyLockInterface(), entry->body);
        if (!lock.Succeeded()) {
            Fail(PhysicsError::BackendFailure, "Unable to lock body for property update");
            return status;
        }
        auto &body = lock.GetBody();
        if (kind != BodyKind::Static && !body.GetMotionPropertiesUnchecked()) {
            Fail(PhysicsError::Unsupported, "This static concave body has no motion state");
            return status;
        }
        const auto deltaCOM   = shape->GetCenterOfMass() - entry->shape->GetCenterOfMass();
        auto      &api        = native->GetBodyInterfaceNoLock();
        const auto motionType = kind == BodyKind::Static    ? JPH::EMotionType::Static
                                : kind == BodyKind::Dynamic ? JPH::EMotionType::Dynamic
                                                            : JPH::EMotionType::Kinematic;
        if (kind == BodyKind::Static)
            api.SetMotionType(entry->body, motionType, JPH::EActivation::DontActivate);
        api.SetShape(entry->body, shape, false, JPH::EActivation::DontActivate);
        api.SetMotionType(entry->body, motionType, JPH::EActivation::DontActivate);
        api.SetObjectLayer(entry->body, kind == BodyKind::Static && !entry->desc.isTrigger ? 0 : 1);
        if (kind != BodyKind::Static) {
            auto properties = shape->GetMassProperties();
            properties.ScaleToMass(mass);
            for (uint32_t axis = 0; axis < 3; ++axis) {
                if (entry->desc.inertia[axis] > 0)
                    properties.mInertia(axis, axis) = entry->desc.inertia[axis];
            }
            auto *motion = body.GetMotionProperties();
            motion->SetMassProperties(motion->GetAllowedDOFs(), properties);
            api.SetMotionQuality(entry->body, options.enableCCD && entry->desc.enableCCD && kind == BodyKind::Dynamic
                                                  ? JPH::EMotionQuality::LinearCast
                                                  : JPH::EMotionQuality::Discrete);
            api.ActivateBody(entry->body);
        }
        for (auto *constraint : connected)
            constraint->constraint->NotifyShapeChanged(entry->body, deltaCOM);
        entry->shape              = std::move(shape);
        entry->desc.shape         = geometry;
        entry->desc.preparedShape = std::move(prepared);
        entry->desc.kind          = kind;
        entry->desc.mass          = mass;
        if (kind != BodyKind::Kinematic)
            entry->kinematicTarget.reset();
        EraseContacts(id);
        return status = {};
    }

    bool JoltWorld::DestroyObject(PhysicsObjectId id)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry) {
            return Fail(PhysicsError::InvalidArgument, "Invalid object handle");
        }
        if (entry->kind == Kind::Constraint) {
            return DestroyConstraint(id);
        }
        if (entry->kind == Kind::Character) {
            return DestroyCharacter(id);
        }
        if (entry->kind == Kind::Material) {
            return DestroyMaterial(id);
        }
        std::vector<PhysicsObjectId> connected;
        for (const auto &candidate : slots) {
            if (candidate && candidate->kind == Kind::Constraint &&
                (candidate->constraintDesc.bodyA == id || candidate->constraintDesc.bodyB == id)) {
                connected.push_back(candidate->id);
            }
        }
        for (auto constraint : connected) {
            DestroyConstraint(constraint);
        }
        EraseContacts(id);
        nativeIds.erase(entry->body.GetIndexAndSequenceNumber());
        if (entry->added)
            native->GetBodyInterface().RemoveBody(entry->body);
        native->GetBodyInterface().DestroyBody(entry->body);
        Release(id);
        --stats.bodyCount;
        return true;
    }
    bool JoltWorld::HasObject(PhysicsObjectId id) const
    {
        return Resolve(id) != nullptr;
    }

    bool JoltWorld::GetBodyTransform(PhysicsObjectId id, Transform &out) const
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body) {
            return Fail(PhysicsError::InvalidArgument, "Invalid body handle");
        }
        out = entry->current;
        return true;
    }
    bool JoltWorld::SetBodyTransform(PhysicsObjectId id, const Transform &transform)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body || !ValidTransform(transform)) {
            return Fail(PhysicsError::InvalidArgument, "Invalid body transform or handle");
        }
        if (transform.scale.x != entry->current.scale.x || transform.scale.y != entry->current.scale.y ||
            transform.scale.z != entry->current.scale.z) {
            auto shape = Cook(entry->desc.shape, transform.scale);
            if (!shape) {
                return false;
            }
            JPH::BodyLockWrite lock(native->GetBodyLockInterface(), entry->body);
            if (!lock.Succeeded()) {
                return Fail(PhysicsError::BackendFailure, "Unable to lock body for scale update");
            }
            // The host serializes world edits with Step; the no-lock interface reuses this body lock.
            native->GetBodyInterfaceNoLock().SetShape(entry->body, shape, false, JPH::EActivation::Activate);
            const auto deltaCOM = shape->GetCenterOfMass() - entry->shape->GetCenterOfMass();
            for (const auto &candidate : slots) {
                if (candidate && candidate->kind == Kind::Constraint &&
                    (candidate->constraintDesc.bodyA == id || candidate->constraintDesc.bodyB == id)) {
                    candidate->constraint->NotifyShapeChanged(entry->body, deltaCOM);
                }
            }
            if (!lock.GetBody().IsStatic()) {
                auto mass = shape->GetMassProperties();
                mass.ScaleToMass(entry->desc.mass);
                for (uint32_t axis = 0; axis < 3; ++axis) {
                    if (entry->desc.inertia[axis] > 0) {
                        mass.mInertia(axis, axis) = entry->desc.inertia[axis];
                    }
                }
                auto *motion = lock.GetBody().GetMotionProperties();
                motion->SetMassProperties(motion->GetAllowedDOFs(), mass);
            }
            entry->shape = shape;
        }
        native->GetBodyInterface().SetPositionAndRotation(entry->body, ToJolt(transform.translation), ToJolt(transform.rotation),
                                                          JPH::EActivation::Activate);
        entry->previous = entry->current = entry->desc.transform = transform;
        entry->kinematicTarget.reset();
        EraseContacts(id);
        return true;
    }
    bool JoltWorld::SetKinematicTarget(PhysicsObjectId id, const Transform &target)
    {
        if (!Ready())
            return false;
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body || entry->desc.kind != BodyKind::Kinematic || !ValidTransform(target)) {
            return Fail(PhysicsError::InvalidArgument, "Invalid kinematic target or handle");
        }
        if (target.scale.x != entry->current.scale.x || target.scale.y != entry->current.scale.y || target.scale.z != entry->current.scale.z) {
            auto scaled  = entry->current;
            scaled.scale = target.scale;
            if (!SetBodyTransform(id, scaled))
                return false;
        }
        entry->kinematicTarget = target;
        return true;
    }
    void JoltWorld::SetInterpolationAlpha(float alpha)
    {
        interpolationAlpha = Finite(alpha) ? std::clamp(alpha, 0.f, 1.f) : 0.f;
    }
    bool JoltWorld::GetInterpolatedTransform(PhysicsObjectId id, Transform &out) const
    {
        if (!GetBodyTransform(id, out)) {
            return false;
        }
        auto &entry     = *Resolve(id);
        out.translation = entry.previous.translation * (1 - interpolationAlpha) + entry.current.translation * interpolationAlpha;
        out.rotation    = FromJolt(ToJolt(entry.previous.rotation).SLERP(ToJolt(entry.current.rotation), interpolationAlpha));
        return true;
    }
    bool JoltWorld::GetBodyVelocity(PhysicsObjectId id, Vector3 &linear, Vector3 &angular) const
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body) {
            return Fail(PhysicsError::InvalidArgument, "Invalid body handle");
        }
        JPH::Vec3 l, a;
        native->GetBodyInterface().GetLinearAndAngularVelocity(entry->body, l, a);
        linear  = FromJolt(l);
        angular = FromJolt(a);
        return true;
    }
    bool JoltWorld::SetBodyVelocity(PhysicsObjectId id, const Vector3 &linear, const Vector3 &angular)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body || entry->desc.kind == BodyKind::Static || !Finite(linear) || !Finite(angular)) {
            return Fail(PhysicsError::InvalidArgument, "Invalid velocity or body");
        }
        native->GetBodyInterface().SetLinearAndAngularVelocity(entry->body, ToJolt(linear), ToJolt(angular));
        entry->kinematicTarget.reset();
        return true;
    }
    bool JoltWorld::ApplyForce(PhysicsObjectId id, const Vector3 &force, const Vector3 &atWorldPos)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body || entry->desc.kind != BodyKind::Dynamic || !Finite(force) || !Finite(atWorldPos)) {
            return Fail(PhysicsError::InvalidArgument, "Force requires a dynamic body and finite vectors");
        }
        native->GetBodyInterface().AddForce(entry->body, ToJolt(force), ToJolt(atWorldPos));
        return true;
    }
    bool JoltWorld::ApplyImpulse(PhysicsObjectId id, const Vector3 &impulse, const Vector3 &atWorldPos)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body || entry->desc.kind != BodyKind::Dynamic || !Finite(impulse) || !Finite(atWorldPos)) {
            return Fail(PhysicsError::InvalidArgument, "Impulse requires a dynamic body and finite vectors");
        }
        native->GetBodyInterface().AddImpulse(entry->body, ToJolt(impulse), ToJolt(atWorldPos));
        return true;
    }
    void JoltWorld::RefreshBody(Entry &entry)
    {
        EraseContacts(entry.id);
        native->GetBodyInterface().InvalidateContactCache(entry.body);
        native->GetBodyInterface().ActivateBody(entry.body);
    }
    bool JoltWorld::SetBodyFilter(PhysicsObjectId id, const CollisionFilter &filter)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body) {
            return Fail(PhysicsError::InvalidArgument, "Invalid body handle");
        }
        entry->filter = entry->desc.filter = filter;
        RefreshBody(*entry);
        return true;
    }
    void JoltWorld::ApplyMaterial(Entry &entry)
    {
        auto &api = native->GetBodyInterface();
        api.SetFriction(entry.body, entry.material.dynamicFriction);
        api.SetRestitution(entry.body, entry.material.restitution);
        JPH::BodyLockWrite lock(native->GetBodyLockInterface(), entry.body);
        if (lock.Succeeded() && !lock.GetBody().IsStatic()) {
            auto *motion = lock.GetBody().GetMotionProperties();
            motion->SetLinearDamping(entry.desc.linearDamping.x != 0 ? entry.desc.linearDamping.x : entry.material.linearDamping);
            motion->SetAngularDamping(entry.desc.angularDamping.x != 0 ? entry.desc.angularDamping.x : entry.material.angularDamping);
        }
    }
    bool JoltWorld::SetBodyDamping(PhysicsObjectId id, float linear, float angular)
    {
        if (!Ready())
            return false;
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body || !Finite(linear) || !Finite(angular) || linear < 0 || angular < 0)
            return Fail(PhysicsError::InvalidArgument, "Invalid body damping");
        JPH::BodyLockWrite lock(native->GetBodyLockInterface(), entry->body);
        if (!lock.Succeeded())
            return Fail(PhysicsError::BackendFailure, "Unable to lock body for damping update");
        if (auto *motion = lock.GetBody().GetMotionPropertiesUnchecked()) {
            motion->SetLinearDamping(linear);
            motion->SetAngularDamping(angular);
        }
        entry->desc.linearDamping  = Vector3(linear, linear, linear);
        entry->desc.angularDamping = Vector3(angular, angular, angular);
        return true;
    }

    bool JoltWorld::SetBodyMaterial(PhysicsObjectId id, PhysicsMaterialId material)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body) {
            return Fail(PhysicsError::InvalidArgument, "Invalid body handle");
        }
        PhysicsMaterialData data;
        if (material != INVALID_PHYSICS_OBJECT_ID && !GetMaterial(material, data)) {
            return false;
        }
        entry->desc.material = material;
        entry->material      = data;
        ApplyMaterial(*entry);
        RefreshBody(*entry);
        return true;
    }
    bool JoltWorld::Wake(PhysicsObjectId id)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Body) {
            return Fail(PhysicsError::InvalidArgument, "Invalid body handle");
        }
        if (options.maxActiveBodies && !native->GetBodyInterface().IsActive(entry->body) && GetStats().activeBodyCount >= options.maxActiveBodies) {
            return Fail(PhysicsError::CapacityExceeded, "Active body budget exceeded");
        }
        native->GetBodyInterface().ActivateBody(entry->body);
        return true;
    }
    PhysicsStatus JoltWorld::SetOptions(const PhysicsOptions &value)
    {
        if (!Ready()) {
            return status;
        }
        if (value.allowDynamicTriangleMesh) {
            Fail(PhysicsError::Unsupported, "Dynamic concave meshes are unsupported");
            return status;
        }
        if ((value.maxBodies && value.maxBodies < stats.bodyCount) || (value.maxActiveBodies && value.maxActiveBodies < GetStats().activeBodyCount)) {
            Fail(PhysicsError::CapacityExceeded, "Budgets are below current usage");
            return status;
        }
        options = value;
        for (const auto &entry : slots) {
            if (!entry || entry->kind != Kind::Body || entry->desc.kind == BodyKind::Static) {
                continue;
            }
            native->GetBodyInterface().SetMotionQuality(entry->body, value.enableCCD && entry->desc.enableCCD && entry->desc.kind == BodyKind::Dynamic
                                                                         ? JPH::EMotionQuality::LinearCast
                                                                         : JPH::EMotionQuality::Discrete);
            {
                JPH::BodyLockWrite lock(native->GetBodyLockInterface(), entry->body);
                if (lock.Succeeded()) {
                    lock.GetBody().SetAllowSleeping(value.enableSleep && entry->desc.allowSleep);
                }
            }
            if (!value.enableSleep) {
                native->GetBodyInterface().ActivateBody(entry->body);
            }
        }
        return status;
    }
} // namespace sky::phy
