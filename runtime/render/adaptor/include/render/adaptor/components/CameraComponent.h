//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Matrix4.h>
#include <framework/world/Component.h>

namespace sky {
    class SceneView;

    enum class ProjectType : uint32_t { ORTHOGONAL, PROJECTIVE };

    class CameraComponent : public ComponentBase {
    public:
        CameraComponent()  = default;
        ~CameraComponent() override;

        static void Reflect(SerializationContext *context);

        void Perspective(float near, float far, float fov);
        void Otho(float height);

        void SetAspect(uint32_t width, uint32_t height);

        void Tick(float time) override;
        void OnActive() override;
        void OnDeActive() override;

        void SaveJson(JsonOutputArchive &ar) const override;
        void LoadJson(JsonInputArchive &ar) override;

    private:
        void ShutDown();

        // data
        float near   = 0.1f;
        float far    = 100.f;
        float fov    = 60;
        float aspect = 1.f;
        float othoH  = 1.f;
        ProjectType type = ProjectType::PROJECTIVE;

        // status
        SceneView *sceneView = nullptr;
    };

} // namespace sky
