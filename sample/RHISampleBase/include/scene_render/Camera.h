//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <memory>
#include <core/math/Matrix4.h>

namespace sky::rhi {

    struct CameraData {
        Matrix4 vp;
        Vector4 position;
    };

    class Camera {
    public:
        Camera() = default;
        ~Camera() = default;

        void SetTransform(const Matrix4 &matrix);
        void MakeProjective(float fov, float aspect, float near, float far);
        void Update();
        const CameraData &GetData() const { return data; }
    private:
        Matrix4 project;
        Matrix4 transform;
        Matrix4 view;
        CameraData data;
    };
    using CameraPtr = std::shared_ptr<Camera>;
}