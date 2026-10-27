//
// Created on 2026/09/23.
//

#pragma once

#include <framework/interface/ITransformEvent.h>
#include <framework/serialization/ArrayVisitor.h>
#include <framework/world/Component.h>

#include <physics/PhysicsBody.h>
#include <physics/PhysicsObjectId.h>
#include <physics/PhysicsStatus.h>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::phy {

    class IPhysicsWorld;

    struct PhysicsBodyData {
        float    mass = 1.f;
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
    class PhysicsBodyComponent : public ComponentAdaptor<PhysicsBodyData>, private ITransformEvent {
    public:
        PhysicsBodyComponent()           = default;
        ~PhysicsBodyComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(PhysicsBodyComponent)

        bool  SetMass(float mass);
        float GetMass() const
        {
            return data.mass;
        }

        bool     SetKind(BodyKind kind);
        BodyKind GetKind() const
        {
            return data.kind;
        }

        bool             SetShape(const ShapeDesc &shape);
        const ShapeDesc &GetShape() const
        {
            return data.shape;
        }

        PhysicsStatus GetLastStatus() const
        {
            return status;
        }
        void            CollectAssetReferences(std::vector<ComponentAssetReference> &out) const override;
        PhysicsObjectId GetObjectId() const
        {
            return objectId;
        }

    private:
        bool TryAttachToWorld(std::string &error) override;
        void OnDetachFromWorld() override;
        void PostTick(float time) override;
        void OnTransformChanged(const Transform &global, const Transform &local) override;

        IPhysicsWorld *GetPhysicsWorld() const;

        void            DestroyBody();
        bool            Apply(const PhysicsBodyData &candidate);
        PhysicsObjectId objectId = INVALID_PHYSICS_OBJECT_ID;
        PhysicsStatus   status;
        bool            writingTransform = false;
    };

} // namespace sky::phy
