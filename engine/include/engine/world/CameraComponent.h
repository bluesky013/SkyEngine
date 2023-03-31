//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Matrix4.h>
#include <engine/base/Component.h>

namespace sky {

    enum class ProjectType : uint32_t { ORTHOGONAL, PROJECTIVE };

    class CameraComponent : public Component {
    public:
        CameraComponent()  = default;
        ~CameraComponent() = default;

        TYPE_RTTI_WITH_VT(CameraComponent)

        static void Reflect();

        void Perspective(float near, float far, float fov, float aspect);

        void Otho(float height);

        void UpdateProjection();

        void OnTick(float time) override;

        void OnActive() override;

        void OnDestroy() override;

        void Save(JsonOutputArchive &ar) const override;
        void Load(JsonInputArchive &ar) override;

    private:
        float near   = 0.1f;
        float far    = 100.f;
        float fov    = 60;
        float aspect = 1.f;
        float othoH  = 1.f;
        ProjectType type;
        Matrix4    projection;
    };

} // namespace sky
