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

        COMPONENT_RUNTIME_INFO(CameraComponent)

        static void Reflect(SerializationContext *context);

        void Perspective(float near, float far, float fov);
        void Otho(float h);

        void SetAspect(uint32_t width, uint32_t height);

        void Tick(float time) override;
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void SaveJson(JsonOutputArchive &ar) const override;
        void LoadJson(JsonInputArchive &ar) override;

        const Matrix4 &GetProject() const;
        const Matrix4 &GetView() const;
    private:
        void ShutDown();

        // data
        float near   = 0.1f;
        float far    = 100.f;
        float fov    = 60;
        float aspect = 1.f;
        float othoH  = 25.f;
        uint32_t width = 1;
        uint32_t height = 1;
        ProjectType type = ProjectType::PROJECTIVE;

        // status
        SceneView *sceneView = nullptr;
    };

} // namespace sky
