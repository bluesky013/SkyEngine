//
// Created by Zach Lee on 2022/7/28.
//

#pragma once
#include <memory>
#include <render/RenderView.h>

namespace sky {

    enum class ProjectType : uint8_t {
        PERSPECTIVE,
        ORTHOGONAL
    };

    class RenderCamera {
    public:
        ~RenderCamera() = default;

        inline bool IsActive() const { return active; }

        inline RDViewPtr GetView() const { return renderView; }

        void SetTransform(const Matrix4& transform);

        void SetFov(float value);

        void SetAspect(float value);

        void SetNearFar(float near, float far);

        void UpdateProjection();

        bool AspectFromViewport() const;

    private:
        friend class CameraFeature;
        RenderCamera() = default;

        void Init();
        RDViewPtr renderView;
        ProjectType projectType = ProjectType::PERSPECTIVE;
        float fov = 60.f;
        float aspect = 3 / 4.f;
        float near = 0.01f;
        float far = 100.f;
        bool active = true;
        bool dirty = true;
        bool autoAspect = true;
    };
}
