//
// Created on 2026/09/23.
//

#pragma once

#include <physics/PhysicsObjectId.h>

#include <core/math/Vector3.h>

#include <vector>

namespace sky::phy {

    enum class PhysicsEventType : uint8_t {
        ContactBegin = 0,
        ContactStay,
        ContactEnd,
        TriggerEnter,
        TriggerExit
    };

    // Backend-neutral contact/trigger event. Payload carries engine types only so it can be replayed
    // and replicated without a backend or render dependency.
    struct PhysicsEvent {
        PhysicsEventType type = PhysicsEventType::ContactBegin;

        PhysicsObjectId a = INVALID_PHYSICS_OBJECT_ID;
        PhysicsObjectId b = INVALID_PHYSICS_OBJECT_ID;

        Vector3 point = VEC3_ZERO;
        Vector3 normal = VEC3_ZERO;
        float   impulse = 0.f;
    };

    // Consumers implement this to receive the events produced by a step, or poll the world directly.
    class IPhysicsEventListener {
    public:
        virtual ~IPhysicsEventListener() = default;
        virtual void OnPhysicsEvents(const std::vector<PhysicsEvent> &events) = 0;
    };

} // namespace sky::phy
