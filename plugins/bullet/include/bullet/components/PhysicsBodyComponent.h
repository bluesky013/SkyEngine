//
// Created on 2026/09/23.
//

#pragma once

#include <framework/world/Component.h>
#include <framework/serialization/ArrayVisitor.h>

#include <physics/PhysicsBody.h>
#include <physics/PhysicsObjectId.h>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::phy {

    class IPhysicsWorld;

    struct PhysicsBodyData {
        float mass = 1.f;
        BodyKind kind = BodyKind::Static;

        bool enableCCD      = false;
        bool gravityEnabled = true;
        bool allowSleep     = true;
        bool isTrigger      = false;

        ShapeDesc       shape;
        CollisionFilter filter;
    };

    // Actor component that backs a physics body with a stable handle. The component never stores a
    // backend pointer, so detaching is safe and the body can be recreated on demand.
    class PhysicsBodyComponent : public ComponentAdaptor<PhysicsBodyData> {
    public:
        PhysicsBodyComponent() = default;
        ~PhysicsBodyComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(PhysicsBodyComponent)

        void SetMass(float mass);
        float GetMass() const { return data.mass; }

        void SetKind(BodyKind kind);
        BodyKind GetKind() const { return data.kind; }

        void SetShape(const ShapeDesc &shape);
        const ShapeDesc &GetShape() const { return data.shape; }

        void ShapeChanged();

        PhysicsObjectId GetObjectId() const { return objectId; }

    private:
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
        void Tick(float time) override;

        IPhysicsWorld *GetPhysicsWorld() const;

        void CreateBody();
        void DestroyBody();
        void Recreate();

        PhysicsObjectId objectId = INVALID_PHYSICS_OBJECT_ID;
    };

} // namespace sky::phy
