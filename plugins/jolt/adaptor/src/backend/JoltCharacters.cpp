//
// Created on 2026/10/26.
//

#include "JoltConversion.h"
#include "JoltWorld.h"

#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace sky::phy {
    class CharacterBodyFilter final : public JPH::BodyFilter {
    public:
        CharacterBodyFilter(const JoltWorld &world, const CollisionFilter &filter) : world(world), filter(filter)
        {
        }
        bool ShouldCollide(const JPH::BodyID &id) const override
        {
            auto *entry = world.FromNative(id);
            return entry && entry->kind == JoltWorld::Kind::Body && !entry->desc.isTrigger && FilterAccepts(filter, entry->filter);
        }

    private:
        const JoltWorld &world;
        CollisionFilter  filter;
    };

    PhysicsObjectId JoltWorld::CreateCharacter(const CharacterDesc &desc)
    {
        if (!Ready()) {
            return {};
        }
        if (!ValidTransform(desc.transform) || !Finite(desc.radius) || desc.radius <= 0 || !Finite(desc.height) || desc.height < 0 ||
            !Finite(desc.stepHeight) || desc.stepHeight < 0 || !Finite(desc.slopeLimit) || desc.slopeLimit < 0 || desc.slopeLimit >= 90 ||
            !Finite(desc.gravity) || desc.gravity < 0 || !Finite(desc.up) || std::abs(ToJolt(desc.up).LengthSq() - 1.f) > 0.001f) {
            Fail(PhysicsError::InvalidArgument, "Invalid character descriptor");
            return {};
        }
        if (desc.transform.scale.x != desc.transform.scale.y || desc.transform.scale.x != desc.transform.scale.z) {
            Fail(PhysicsError::Unsupported, "Characters require uniform scale");
            return {};
        }
        if (stats.characterCount >= descriptor.limits.characters || native->GetNumBodies() >= descriptor.limits.bodies) {
            Fail(PhysicsError::CapacityExceeded, "Character or native body capacity exceeded");
            return {};
        }
        ShapeDesc capsule;
        capsule.type   = ShapeType::Capsule;
        capsule.radius = desc.radius;
        capsule.height = desc.height;
        auto shape     = Cook(capsule, desc.transform.scale);
        if (!shape) {
            return {};
        }
        auto localUp = ToJolt(desc.transform.rotation).Conjugated() * ToJolt(desc.up);
        auto rotated = JPH::RotatedTranslatedShapeSettings(JPH::Vec3::sZero(), JPH::Quat::sFromTo(JPH::Vec3::sAxisY(), localUp), shape).Create();
        if (!rotated.IsValid()) {
            Fail(PhysicsError::InvalidArgument, "Invalid character shape");
            return {};
        }
        shape               = rotated.Get();
        auto &entry         = Allocate(Kind::Character);
        entry.characterDesc = desc;
        entry.shape         = shape;
        entry.filter        = desc.filter;
        entry.previous = entry.current = desc.transform;
        JPH::CharacterVirtualSettings settings;
        settings.mShape = settings.mInnerBodyShape = shape;
        settings.mInnerBodyLayer                   = 1;
        settings.mUp                               = ToJolt(desc.up);
        settings.mSupportingVolume                 = JPH::Plane(localUp, 0.f);
        settings.mMaxSlopeAngle                    = JPH::DegreesToRadians(desc.slopeLimit);
        entry.character = std::make_unique<JPH::CharacterVirtual>(&settings, ToJolt(desc.transform.translation), ToJolt(desc.transform.rotation),
                                                                  entry.id.index, native.get());
        entry.body      = entry.character->GetInnerBodyID();
        if (entry.body.IsInvalid()) {
            auto id = entry.id;
            Release(id);
            Fail(PhysicsError::CapacityExceeded, "Character proxy allocation failed");
            return {};
        }
        nativeIds[entry.body.GetIndexAndSequenceNumber()] = entry.id;
        native->GetBodyInterface().SetCollisionGroup(entry.body, JPH::CollisionGroup(pairFilter, uint32_t(entry.id.index), entry.id.generation));
        entry.character->SetListener(this);
        entry.character->SetCharacterVsCharacterCollision(&characterCollisions);
        characterCollisions.Add(entry.character.get());
        ++stats.characterCount;
        return entry.id;
    }

    bool JoltWorld::OnCharacterContactValidate(const JPH::CharacterVirtual *character, const JPH::CharacterContact &contact)
    {
        auto *a = FromNative(character->GetInnerBodyID());
        auto *b = contact.mCharacterB ? FromNative(contact.mCharacterB->GetInnerBodyID()) : nullptr;
        return a && b && FilterAccepts(a->filter, b->filter);
    }

    bool JoltWorld::DestroyCharacter(PhysicsObjectId id)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Character) {
            return Fail(PhysicsError::InvalidArgument, "Invalid character handle");
        }
        characterCollisions.Remove(entry->character.get());
        EraseContacts(id);
        nativeIds.erase(entry->body.GetIndexAndSequenceNumber());
        Release(id);
        --stats.characterCount;
        return true;
    }
    CharacterMoveResult JoltWorld::MoveCharacter(PhysicsObjectId id, const Vector3 &displacement)
    {
        if (!Ready()) {
            return CharacterMoveResult::Rejected;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Character) {
            Fail(PhysicsError::InvalidArgument, "Invalid character handle");
            return CharacterMoveResult::NotAttached;
        }
        if (!Finite(displacement) || !Finite(entry->displacement + displacement)) {
            Fail(PhysicsError::InvalidArgument, "Invalid character displacement");
            return CharacterMoveResult::Rejected;
        }
        entry->displacement += displacement;
        return CharacterMoveResult::Applied;
    }
    bool JoltWorld::GetCharacterState(PhysicsObjectId id, CharacterState &out) const
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Character) {
            return Fail(PhysicsError::InvalidArgument, "Invalid character handle");
        }
        out.transform = entry->current;
        out.grounded  = entry->character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
        return true;
    }
    bool JoltWorld::SetCharacterTransform(PhysicsObjectId id, const Transform &transform)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Character || !ValidTransform(transform)) {
            return Fail(PhysicsError::InvalidArgument, "Invalid character transform or handle");
        }
        if (transform.scale != entry->current.scale) {
            return Fail(PhysicsError::Unsupported, "Resize characters through SetCharacterCapsule");
        }
        entry->character->SetPosition(ToJolt(transform.translation));
        entry->character->SetRotation(ToJolt(transform.rotation));
        entry->character->SetLinearVelocity(JPH::Vec3::sZero());
        entry->previous = entry->current = entry->characterDesc.transform = transform;
        entry->displacement                                               = VEC3_ZERO;
        EraseContacts(id);
        return true;
    }
    bool JoltWorld::SetCharacterCapsule(PhysicsObjectId id, float radius, float height)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Character) {
            return Fail(PhysicsError::InvalidArgument, "Invalid character handle");
        }
        ShapeDesc capsule;
        capsule.type   = ShapeType::Capsule;
        capsule.radius = radius;
        capsule.height = height;
        auto shape     = Cook(capsule, entry->current.scale);
        if (!shape) {
            return false;
        }
        auto localUp = ToJolt(entry->current.rotation).Conjugated() * ToJolt(entry->characterDesc.up);
        auto rotated = JPH::RotatedTranslatedShapeSettings(JPH::Vec3::sZero(), JPH::Quat::sFromTo(JPH::Vec3::sAxisY(), localUp), shape).Create();
        if (!rotated.IsValid()) {
            return Fail(PhysicsError::InvalidArgument, "Invalid character shape");
        }
        shape = rotated.Get();
        CharacterBodyFilter filter(*this, entry->filter);
        if (!entry->character->SetShape(shape, 0.02f, {}, {}, filter, {}, allocator)) {
            return Fail(PhysicsError::InvalidState, "Character resize overlaps geometry");
        }
        entry->character->SetInnerBodyShape(shape);
        entry->shape                = shape;
        entry->characterDesc.radius = radius;
        entry->characterDesc.height = height;
        EraseContacts(id);
        return true;
    }
    void JoltWorld::UpdateCharacters(float delta)
    {
        for (auto &entry : slots) {
            if (!entry || entry->kind != Kind::Character) {
                continue;
            }
            auto &character = *entry->character;
            auto  up        = ToJolt(entry->characterDesc.up);
            auto  gravity   = -up * entry->characterDesc.gravity;
            character.UpdateGroundVelocity();
            auto velocity = character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround ? character.GetGroundVelocity()
                                                                                                     : up * character.GetLinearVelocity().Dot(up);
            velocity += gravity * delta + ToJolt(entry->displacement) / delta;
            entry->displacement = VEC3_ZERO;
            character.SetLinearVelocity(velocity);
            JPH::CharacterVirtual::ExtendedUpdateSettings settings;
            settings.mWalkStairsStepUp     = up * entry->characterDesc.stepHeight;
            settings.mStickToFloorStepDown = -up * entry->characterDesc.stepHeight;
            CharacterBodyFilter filter(*this, entry->filter);
            character.ExtendedUpdate(delta, gravity, settings, {}, {}, filter, {}, allocator);
        }
    }
} // namespace sky::phy
