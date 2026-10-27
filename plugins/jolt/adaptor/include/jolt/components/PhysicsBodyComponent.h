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
    enum class PhysicsShapeSource : uint8_t { Inline, Asset };

    struct PhysicsBodyData {
        float    mass = 1.f;
        BodyKind kind = BodyKind::Static;

        bool enableCCD      = false;
        bool gravityEnabled = true;
        bool allowSleep     = true;
        bool isTrigger      = false;

        ShapeDesc          shape;
        CollisionFilter    filter;
        PhysicsShapeSource shapeSource = PhysicsShapeSource::Inline;
        Uuid               collisionAsset;
        Uuid               materialAsset;
        float              linearDamping  = 0;
        float              angularDamping = 0;
    };

    // Actor component that backs a physics body with a stable handle. The component never stores a
    // backend pointer, so detaching is safe and the body can be recreated on demand.
    class PhysicsBodyComponent : public ComponentAdaptor<PhysicsBodyData>, private ITransformEvent {
    public:
        PhysicsBodyComponent();
        ~PhysicsBodyComponent() override;

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
        void                 CollectAssetReferences(std::vector<ComponentAssetReference> &out) const override;
        ComponentPreparation PrepareForWorld(World &world, std::string &error) override;
        void                 CancelPreparation() override;
        bool                 KeepWorldResidency() const override
        {
            return data.kind != BodyKind::Static;
        }
        bool               SetShapeSource(PhysicsShapeSource source);
        PhysicsShapeSource GetShapeSource() const
        {
            return data.shapeSource;
        }
        bool        SetCollisionAsset(const Uuid &asset);
        const Uuid &GetCollisionAsset() const
        {
            return data.collisionAsset;
        }
        bool        SetMaterialAsset(const Uuid &asset);
        const Uuid &GetMaterialAsset() const
        {
            return data.materialAsset;
        }
        float GetLinearDamping() const
        {
            return data.linearDamping;
        }
        float GetAngularDamping() const
        {
            return data.angularDamping;
        }
        bool             SetLinearDamping(float value);
        bool             SetAngularDamping(float value);
        const ShapeDesc &GetResolvedShape() const;

        PhysicsObjectId GetObjectId() const
        {
            return objectId;
        }

    private:
        bool TryAttachToWorld(std::string &error) override;
        void OnDetachFromWorld() override;
        void PostTick(float time) override;
        void Tick(float time) override;
        void OnTransformChanged(const Transform &global, const Transform &local) override;

        IPhysicsWorld *GetPhysicsWorld() const;

        void                             DestroyBody();
        bool                             Apply(const PhysicsBodyData &candidate);
        bool                             BindPrepared();
        std::shared_ptr<const ShapeDesc> installedShape;
        struct Resources;
        std::unique_ptr<Resources> resources;
        PhysicsMaterialId          materialId     = INVALID_PHYSICS_OBJECT_ID;
        bool                       pendingBinding = false;

        PhysicsObjectId objectId = INVALID_PHYSICS_OBJECT_ID;
        PhysicsStatus   status;
        bool            writingTransform = false;
    };

} // namespace sky::phy
