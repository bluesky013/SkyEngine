//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Matrix4.h>
#include <framework/world/Component.h>

namespace sky {
    class SceneView;

    enum class ProjectType : uint32_t { ORTHOGONAL, PROJECTIVE };

    class CameraComponent : public Component {
    public:
        CameraComponent()  = default;
        ~CameraComponent() override = default;

        TYPE_RTTI_WITH_VT(CameraComponent)

        static void Reflect();

        void Perspective(float near, float far, float fov);
        void Otho(float height);

        void SetAspect(uint32_t width, uint32_t height);

        void OnTick(float time) override;
        void OnActive() override;
        void OnDestroy() override;

        void Save(JsonOutputArchive &ar) const override;
        void Load(JsonInputArchive &ar) override;

    private:
        // data
        float near   = 0.1f;
        float far    = 100.f;
        float fov    = 60;
        float aspect = 1.f;
        float othoH  = 1.f;
        ProjectType type = ProjectType::PROJECTIVE;

        // status
        bool dirty = true;
        SceneView *sceneView = nullptr;
    };

} // namespace sky
