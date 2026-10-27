//
// Created on 2026/10/26.
//

#include "JoltConversion.h"
#include "JoltWorld.h"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

#include <algorithm>
#include <chrono>
#include <set>

namespace sky::phy {
    static float Combine(float a, float b, PhysicsCombineMode x, PhysicsCombineMode y)
    {
        switch (std::max(x, y)) {
        case PhysicsCombineMode::Min: return std::min(a, b);
        case PhysicsCombineMode::Multiply: return a * b;
        case PhysicsCombineMode::Max: return std::max(a, b);
        default: return (a + b) * 0.5f;
        }
    }
    void JoltWorld::RecordContact(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &m, JPH::ContactSettings &settings)
    {
        auto *first  = FromNative(a.GetID());
        auto *second = FromNative(b.GetID());
        if (!first || !second) {
            return;
        }
        auto &ma                      = first->material;
        auto &mb                      = second->material;
        settings.mCombinedFriction    = Combine(ma.dynamicFriction, mb.dynamicFriction, ma.frictionCombine, mb.frictionCombine);
        settings.mCombinedRestitution = Combine(ma.restitution, mb.restitution, ma.restitutionCombine, mb.restitutionCombine);
        PhysicsEvent event;
        event.a      = first->id;
        event.b      = second->id;
        event.type   = a.IsSensor() || b.IsSensor() ? PhysicsEventType::TriggerEnter : PhysicsEventType::ContactBegin;
        event.normal = FromJolt(m.mWorldSpaceNormal);
        if (!m.mRelativeContactPointsOn1.empty()) {
            event.point = FromJolt(m.GetWorldSpaceContactPointOn1(0));
        }
        if (event.b < event.a) {
            std::swap(event.a, event.b);
            event.normal = -event.normal;
        }
        ContactKey      key{a.GetID().GetIndexAndSequenceNumber(), m.mSubShapeID1.GetValue(), b.GetID().GetIndexAndSequenceNumber(),
                       m.mSubShapeID2.GetValue()};
        std::lock_guard lock(contactMutex);
        contacts[key] = {event, false};
    }
    void JoltWorld::OnContactAdded(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &m, JPH::ContactSettings &s)
    {
        RecordContact(a, b, m, s);
    }
    void JoltWorld::OnContactPersisted(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &m, JPH::ContactSettings &s)
    {
        RecordContact(a, b, m, s);
    }
    void JoltWorld::OnContactRemoved(const JPH::SubShapeIDPair &pair)
    {
        ContactKey key{pair.GetBody1ID().GetIndexAndSequenceNumber(), pair.GetSubShapeID1().GetValue(), pair.GetBody2ID().GetIndexAndSequenceNumber(),
                       pair.GetSubShapeID2().GetValue()};
        std::lock_guard lock(contactMutex);
        auto            found = contacts.find(key);
        if (found != contacts.end()) {
            found->second.removed = true;
        }
    }
    void JoltWorld::EraseContacts(PhysicsObjectId id)
    {
        std::lock_guard lock(contactMutex);
        for (auto it = contacts.begin(); it != contacts.end();) {
            if (it->second.event.a == id || it->second.event.b == id) {
                it = contacts.erase(it);
            } else {
                ++it;
            }
        }
    }
    void JoltWorld::PublishContacts()
    {
        std::map<Pair, PhysicsEvent> currentPairs;
        {
            std::lock_guard lock(contactMutex);
            for (auto it = contacts.begin(); it != contacts.end();) {
                auto &contact = it->second;
                auto *a       = Resolve(contact.event.a);
                auto *b       = Resolve(contact.event.b);
                bool  valid   = a && b && FilterAccepts(a->filter, b->filter) && exclusions.find({a->id, b->id}) == exclusions.end();
                // Jolt retires sleeping manifolds; their object-pair contact remains until either body changes.
                if (valid && contact.removed) {
                    valid = !native->GetBodyInterface().IsActive(a->body) && !native->GetBodyInterface().IsActive(b->body);
                }
                if (!valid) {
                    it = contacts.erase(it);
                    continue;
                }
                currentPairs.emplace(Pair{contact.event.a, contact.event.b}, contact.event);
                ++it;
            }
        }
        for (const auto &entry : slots) {
            if (!entry || entry->kind != Kind::Character) {
                continue;
            }
            for (const auto &contact : entry->character->GetActiveContacts()) {
                if (!contact.mHadCollision || contact.mWasDiscarded) {
                    continue;
                }
                auto *other = FromNative(contact.mBodyB);
                if (!other && !contact.mCharacterIDB.IsInvalid()) {
                    for (const auto &candidate : slots) {
                        if (candidate && candidate->character && candidate->character->GetID() == contact.mCharacterIDB) {
                            other = candidate.get();
                            break;
                        }
                    }
                }
                if (!other || !FilterAccepts(entry->filter, other->filter)) {
                    continue;
                }
                PhysicsEvent event;
                event.a      = entry->id;
                event.b      = other->id;
                event.point  = FromJolt(contact.mPosition);
                event.normal = -FromJolt(contact.mContactNormal);
                if (event.b < event.a) {
                    std::swap(event.a, event.b);
                    event.normal = -event.normal;
                }
                currentPairs.emplace(Pair{event.a, event.b}, event);
            }
        }
        std::vector<PhysicsEvent> batch;
        for (const auto &[pair, event] : currentPairs) {
            auto copy = event;
            if (previousPairs.count(pair)) {
                if (copy.type == PhysicsEventType::TriggerEnter) {
                    continue;
                }
                copy.type = PhysicsEventType::ContactStay;
            }
            batch.push_back(copy);
        }
        for (const auto &[pair, event] : previousPairs) {
            if (currentPairs.count(pair)) {
                continue;
            }
            auto copy = event;
            copy.type = event.type == PhysicsEventType::TriggerEnter ? PhysicsEventType::TriggerExit : PhysicsEventType::ContactEnd;
            batch.push_back(copy);
        }
        std::sort(batch.begin(), batch.end(), [](const auto &a, const auto &b) { return std::tie(a.a, a.b, a.type) < std::tie(b.a, b.b, b.type); });
        events.insert(events.end(), batch.begin(), batch.end());
        previousPairs      = std::move(currentPairs);
        stats.contactCount = static_cast<uint32_t>(previousPairs.size());
    }
    void JoltWorld::Synchronize()
    {
        for (auto &entry : slots) {
            if (!entry) {
                continue;
            }
            if (entry->kind == Kind::Body) {
                entry->current.translation = FromJolt(native->GetBodyInterface().GetPosition(entry->body));
                entry->current.rotation    = FromJolt(native->GetBodyInterface().GetRotation(entry->body));
            } else if (entry->kind == Kind::Character) {
                entry->current.translation = FromJolt(entry->character->GetPosition());
                entry->current.rotation    = FromJolt(entry->character->GetRotation());
            }
        }
    }
    PhysicsStatus JoltWorld::Step(float delta)
    {
        if (!Ready()) {
            return status;
        }
        if (!Finite(delta) || delta <= 0) {
            Fail(PhysicsError::InvalidArgument, "Step duration must be finite and positive");
            return status;
        }
        if (options.maxActiveBodies && GetStats().activeBodyCount > options.maxActiveBodies) {
            Fail(PhysicsError::CapacityExceeded, "Active body budget exceeded");
            return status;
        }
        const auto      begin = std::chrono::steady_clock::now();
        std::lock_guard updateLock(executor->updateMutex);
        for (auto &entry : slots) {
            if (entry) {
                entry->previous = entry->current;
                if (entry->kind == Kind::Body && entry->kinematicTarget) {
                    native->GetBodyInterface().MoveKinematic(entry->body, ToJolt(entry->kinematicTarget->translation),
                                                             ToJolt(entry->kinematicTarget->rotation), delta);
                }
            }
        }
        UpdateCharacters(delta);
        auto error = native->Update(delta, 1, &allocator, &executor->jobs);
        if (error != JPH::EPhysicsUpdateError::None) {
            faulted = true;
            events.clear();
            status = {PhysicsError::CapacityExceeded, "Jolt update capacity exhausted (flags=" + std::to_string(static_cast<uint32_t>(error)) +
                                                          "); recreate the world with larger limits"};
            return status;
        }
        if (options.maxActiveBodies && GetStats().activeBodyCount > options.maxActiveBodies) {
            faulted = true;
            events.clear();
            Fail(PhysicsError::CapacityExceeded, "Simulation exceeded the active body budget");
            return status;
        }
        Synchronize();
        PublishContacts();
        ++stats.stepCount;
        stats.lastStepMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        return status;
    }
    PhysicsWorldStats JoltWorld::GetStats() const
    {
        auto result            = stats;
        result.activeBodyCount = 0;
        if (native) {
            for (const auto &entry : slots) {
                if (entry && entry->kind == Kind::Body && native->GetBodyInterface().IsActive(entry->body)) {
                    ++result.activeBodyCount;
                }
            }
        }
        return result;
    }
    void JoltWorld::DrainEvents(std::vector<PhysicsEvent> &out)
    {
        out.insert(out.end(), events.begin(), events.end());
        events.clear();
    }

    bool JoltWorld::CaptureState(PhysicsWorldState &out, PhysicsSnapshotScope scope) const
    {
        if (!Ready()) {
            return false;
        }
        if (scope > PhysicsSnapshotScope::Full) {
            return Fail(PhysicsError::InvalidArgument, "Invalid snapshot scope");
        }
        PhysicsWorldState state;
        state.frame = stats.stepCount;
        state.scope = scope;
        for (const auto &entry : slots) {
            if (!entry || entry->kind != Kind::Body || (scope == PhysicsSnapshotScope::DynamicKinematic && entry->desc.kind == BodyKind::Static)) {
                continue;
            }
            PhysicsBodyState body;
            body.id        = entry->id;
            body.transform = entry->current;
            GetBodyVelocity(entry->id, body.linearVelocity, body.angularVelocity);
            body.asleep = !native->GetBodyInterface().IsActive(entry->body);
            state.bodies.push_back(body);
        }
        out = std::move(state);
        return true;
    }
    bool JoltWorld::RestoreState(const PhysicsWorldState &state)
    {
        if (!Ready()) {
            return false;
        }
        if (state.scope > PhysicsSnapshotScope::Full) {
            return Fail(PhysicsError::InvalidArgument, "Invalid snapshot scope");
        }
        std::set<PhysicsObjectId> seen;
        for (const auto &body : state.bodies) {
            auto *entry = Resolve(body.id);
            if (!entry || entry->kind != Kind::Body || !seen.insert(body.id).second || !ValidTransform(body.transform) ||
                !Finite(body.linearVelocity) || !Finite(body.angularVelocity) || entry->current.scale != body.transform.scale ||
                (state.scope == PhysicsSnapshotScope::DynamicKinematic && entry->desc.kind == BodyKind::Static)) {
                return Fail(PhysicsError::InvalidArgument, "Snapshot contains invalid bodies, transforms or duplicates");
            }
        }
        for (const auto &body : state.bodies) {
            auto &entry = *Resolve(body.id);
            native->GetBodyInterface().SetPositionAndRotation(entry.body, ToJolt(body.transform.translation), ToJolt(body.transform.rotation),
                                                              JPH::EActivation::DontActivate);
            if (entry.desc.kind != BodyKind::Static) {
                native->GetBodyInterface().SetLinearAndAngularVelocity(entry.body, ToJolt(body.linearVelocity), ToJolt(body.angularVelocity));
                if (body.asleep) {
                    native->GetBodyInterface().DeactivateBody(entry.body);
                } else {
                    native->GetBodyInterface().ActivateBody(entry.body);
                }
            }
            native->GetBodyInterface().InvalidateContactCache(entry.body);
            entry.previous = entry.current = body.transform;
        }
        for (auto &entry : slots) {
            if (entry) {
                entry->displacement = VEC3_ZERO;
            }
        }
        contacts.clear();
        previousPairs.clear();
        events.clear();
        stats.contactCount = 0;
        stats.stepCount    = state.frame;
        interpolationAlpha = 0;
        return true;
    }

    class GeometryCollector final : public JPH::DebugRendererSimple {
    public:
        explicit GeometryCollector(PhysicsDebugGeometry &out) : out(out)
        {
        }
        void DrawLine(JPH::RVec3Arg a, JPH::RVec3Arg b, JPH::ColorArg c) override
        {
            const float color[] = {c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
            out.AddLine(FromJolt(a), FromJolt(b), color);
        }
        void DrawTriangle(JPH::RVec3Arg a, JPH::RVec3Arg b, JPH::RVec3Arg c, JPH::ColorArg color, ECastShadow) override
        {
            const float rgba[] = {color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f};
            out.AddTriangle(FromJolt(a), FromJolt(b), FromJolt(c), rgba);
        }
        void DrawText3D(JPH::RVec3Arg, const JPH::string_view &, JPH::ColorArg, float) override
        {
        }

    private:
        PhysicsDebugGeometry &out;
    };
    class GeometryFilter final : public JPH::BodyDrawFilter {
    public:
        GeometryFilter(const JoltWorld &world, uint32_t categories) : world(world), categories(categories)
        {
        }
        bool ShouldDraw(const JPH::Body &body) const override
        {
            auto *entry = world.FromNative(body.GetID());
            return entry && (entry->kind == JoltWorld::Kind::Character ? (categories & PhysicsDebugCategory::Character) != 0
                                                                       : (categories & PhysicsDebugCategory::Shapes) != 0);
        }

    private:
        const JoltWorld &world;
        uint32_t         categories;
    };
    std::mutex &JoltDebugMutex();
    void        JoltWorld::CollectDebugGeometry(uint32_t categories, PhysicsDebugGeometry &out) const
    {
        out.Clear();
        if (!Ready() || categories == 0) {
            return;
        }
        std::lock_guard                lock(JoltDebugMutex());
        GeometryCollector              renderer(out);
        GeometryFilter                 filter(*this, categories);
        JPH::BodyManager::DrawSettings settings;
        settings.mDrawShape       = (categories & (PhysicsDebugCategory::Shapes | PhysicsDebugCategory::Character)) != 0;
        settings.mDrawBoundingBox = false;
        native->DrawBodies(settings, &renderer, &filter);
        if (categories & PhysicsDebugCategory::AABBs) {
            GeometryFilter boundsFilter(*this, PhysicsDebugCategory::Shapes | PhysicsDebugCategory::Character);
            settings.mDrawShape       = false;
            settings.mDrawBoundingBox = true;
            native->DrawBodies(settings, &renderer, &boundsFilter);
        }
        if (categories & PhysicsDebugCategory::Constraints) {
            native->DrawConstraints(&renderer);
        }
        if (categories & PhysicsDebugCategory::Contacts) {
            for (const auto &[pair, event] : previousPairs) {
                renderer.DrawLine(ToJolt(event.point), ToJolt(event.point + event.normal * 0.3f), JPH::Color::sRed);
            }
        }
    }
} // namespace sky::phy
