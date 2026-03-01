//
// Created by Zach Lee on 2026/2/24.
//

#pragma once

#include <editor/framework/EditorActorCreation.h>
#include <core/math/Transform.h>
#include <framework/world/Component.h>
#include <framework/interface/ITransformEvent.h>
#include <render/debug/VolumeRenderer.h>

namespace sky::editor {

    class PVSVolume
        : public ComponentBase
        , public ITransformEvent {
    public:
        COMPONENT_RUNTIME_INFO(PVSVolume)

        static void Reflect(SerializationContext *context);

        PVSVolume() = default;
        ~PVSVolume() override = default;

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        const BoundingBox& GetBounding() const noexcept { return boundingBox; }

    private:
        void DrwBox() noexcept;
        void OnTransformChanged(const Transform& global, const Transform& local) override;

        BoundingBox boundingBox;
        EventBinder<ITransformEvent> transformEvent;
        std::unique_ptr<VolumeRenderer> volumeRender;
    };

    class PVSVolumeCreator : public IActorCreateBase {
    public:
        PVSVolumeCreator() : group("Volume"), name("Precomputed Visibility Volume") {}
        ~PVSVolumeCreator() override = default;

        Name GetGroup() const override { return group; }
        Name GetName() const override { return name; }

        bool OnCreateActor(Actor* actor) override;

    private:
        Name group;
        Name name;
    };

} // namespace sky::editor