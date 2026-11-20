//
// Created by blues on 2024/12/8.
//

#pragma once

#include <core/math/Transform.h>
#include <framework/world/Component.h>
#include <framework/interface/ITransformEvent.h>
#include <render/decal/DecalFeatureProcessor.h>

namespace sky {

    struct DecalComponentData {
        Vector4 color       = {1.f, 1.f, 1.f, 1.f};
        float   blendFactor = 1.f;
    };

    /**
     * Actor component that places a screen-space decal in the world.
     * The decal is an oriented bounding box derived from the actor transform;
     * scaling the actor controls the decal's extents in world space.
     */
    class DecalComponent
        : public ComponentAdaptor<DecalComponentData>
        , public ITransformEvent {
    public:
        DecalComponent() = default;
        ~DecalComponent() override = default;

        COMPONENT_RUNTIME_INFO(DecalComponent)

        static void Reflect(SerializationContext *context);

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

    private:
        void OnTransformChanged(const Transform &global, const Transform &local) override;
        void UpdateDecal(const Transform &transform);

        EventBinder<ITransformEvent> transformEvent;
        DecalInstance *decal = nullptr;
    };

} // namespace sky
