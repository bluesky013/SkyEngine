#include "JoltConversion.h"
#include "JoltWorld.h"

#include <algorithm>
#include <framework/asset/AssetExecutor.h>

namespace sky::phy {
    JoltPreparedShape::~JoltPreparedShape()
    {
        if (accountedBytes) {
            session->preparationBytes.fetch_sub(accountedBytes);
            session->preparationCount.fetch_sub(1);
        }
    }

    class JoltShapeRequest final : public IPhysicsShapeRequest {
    public:
        ShapePreparationState GetState() const override
        {
            return state.load(std::memory_order_acquire);
        }
        PhysicsStatus GetStatus() const override
        {
            return GetState() == ShapePreparationState::Preparing ? PhysicsStatus{PhysicsError::InvalidState, "Shape is preparing"} : status;
        }
        std::shared_ptr<const IPhysicsShape> GetShape() const override
        {
            return GetState() == ShapePreparationState::Ready ? shape : nullptr;
        }
        std::shared_ptr<JoltPreparedShape> shape;
        PhysicsStatus                      status;
        std::atomic<ShapePreparationState> state{ShapePreparationState::Preparing};
    };

    std::shared_ptr<IPhysicsShapeRequest> JoltWorld::PrepareShape(std::shared_ptr<const ShapeDesc> source, const Vector3 &scale)
    {
        if (!source || !Finite(scale) || scale.x <= 0 || scale.y <= 0 || scale.z <= 0) {
            Fail(PhysicsError::InvalidArgument, "Invalid shape preparation input");
            return {};
        }
        std::lock_guard lock(session->preparationMutex);
        std::erase_if(session->preparations, [](const auto &entry) { return entry.expired(); });
        for (const auto &weak : session->preparations) {
            auto request = weak.lock();
            if (request && request->shape->source == source && request->shape->margin == descriptor.collisionMargin &&
                request->shape->scale.x == scale.x && request->shape->scale.y == scale.y && request->shape->scale.z == scale.z)
                return request;
        }
        constexpr size_t               MAX_BYTES = 256u * 1024u * 1024u;
        size_t                         bytes = 0, count = 0;
        std::vector<const ShapeDesc *> pending{source.get()};
        while (!pending.empty()) {
            const auto *shape = pending.back();
            pending.pop_back();
            bytes += sizeof(ShapeDesc) + shape->samples.size() * sizeof(float) + shape->points.size() * sizeof(Vector3);
            if (shape->meshData)
                bytes += shape->meshData->position.size() + shape->meshData->indexRaw.size();
            if (++count > 4096 || bytes > MAX_BYTES / 4 || pending.size() + shape->children.size() > 4096) {
                Fail(PhysicsError::CapacityExceeded, "Shape preparation geometry budget exceeded");
                return {};
            }
            for (const auto &child : shape->children)
                pending.push_back(&child);
        }
        if (session->preparationTasks.load() >= 32 || session->preparationCount.load() >= 1024 ||
            bytes > MAX_BYTES - session->preparationBytes.load()) {
            Fail(PhysicsError::CapacityExceeded, "Shape preparation residency budget exceeded");
            return {};
        }
        auto request                   = std::make_shared<JoltShapeRequest>();
        request->shape                 = std::make_shared<JoltPreparedShape>();
        request->shape->session        = session;
        request->shape->source         = std::move(source);
        request->shape->scale          = scale;
        request->shape->margin         = descriptor.collisionMargin;
        request->shape->accountedBytes = bytes;
        session->preparationBytes.fetch_add(bytes);
        session->preparationCount.fetch_add(1);
        session->preparations.push_back(request);
        session->preparationTasks.fetch_add(1);
        AssetExecutor::Get()->DependentAsync([request] {
            auto &shape  = *request->shape;
            shape.native = CookJoltShape(*shape.source, shape.scale, shape.margin, request->status);
            shape.session->preparationTasks.fetch_sub(1);
            request->state.store(shape.native ? ShapePreparationState::Ready : ShapePreparationState::Failed, std::memory_order_release);
        });
        status = {};
        return request;
    }

    PhysicsStatus JoltWorld::BeginActivation(uint32_t bodies)
    {
        if (!Ready())
            return status;
        if (activation) {
            Fail(PhysicsError::InvalidState, "Activation is already in progress");
        } else if (bodies > descriptor.limits.bodies - native->GetNumBodies() ||
                   (options.maxBodies && bodies > options.maxBodies - stats.bodyCount)) {
            Fail(PhysicsError::CapacityExceeded, "Activation group exceeds body capacity");
        } else {
            activation = true;
            staged.clear();
            status = {};
        }
        return status;
    }

    void JoltWorld::EndActivation(bool commit)
    {
        if (!activation)
            return;
        activation = false;
        for (const auto id : staged) {
            auto *entry = Resolve(id);
            if (!entry)
                continue;
            if (!commit) {
                DestroyObject(id);
                continue;
            }
            native->GetBodyInterface().AddBody(entry->body, entry->desc.startsAsleep || entry->desc.kind == BodyKind::Static
                                                                ? JPH::EActivation::DontActivate
                                                                : JPH::EActivation::Activate);
            entry->added = true;
        }
        staged.clear();
    }

    bool JoltWorld::CanRetire(std::span<const PhysicsObjectId> objects) const
    {
        if (!native || faulted)
            return false;
        const auto contains = [objects](PhysicsObjectId id) { return std::find(objects.begin(), objects.end(), id) != objects.end(); };
        for (const auto &entry : slots) {
            if (!entry || entry->kind != Kind::Constraint)
                continue;
            const auto &constraint = entry->constraintDesc;
            if ((contains(constraint.bodyA) && IsValid(constraint.bodyB) && !contains(constraint.bodyB)) ||
                (contains(constraint.bodyB) && IsValid(constraint.bodyA) && !contains(constraint.bodyA)))
                return false;
        }
        // Host calls are serialized with Step; use live manifolds, not the last published event snapshot.
        for (const auto &[key, contact] : contacts) {
            const auto &event = contact.event;
            if (event.type == PhysicsEventType::TriggerEnter || contains(event.a) == contains(event.b))
                continue;
            const auto *other = Resolve(contains(event.a) ? event.b : event.a);
            if (other && other->kind == Kind::Body && other->desc.kind != BodyKind::Static && !other->desc.isTrigger)
                return false;
        }
        for (const auto &entry : slots) {
            if (!entry || entry->kind != Kind::Character || contains(entry->id))
                continue;
            for (const auto &contact : entry->character->GetActiveContacts()) {
                const auto *body = FromNative(contact.mBodyB);
                if (contact.mHadCollision && !contact.mWasDiscarded && body && contains(body->id))
                    return false;
            }
        }
        return true;
    }
} // namespace sky::phy
