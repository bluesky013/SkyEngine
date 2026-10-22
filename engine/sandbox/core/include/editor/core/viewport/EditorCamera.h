//
// Created on 2026/10/22.
//

#pragma once

#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>

namespace sky::editor {
    struct ViewportControlSettings {
        float       speed           = 5.f;
        float       boost           = 4.f;
        float       sensitivity     = .25f;
        float       translationStep = .5f;
        float       rotationStep    = 15.f;
        float       scaleStep       = .1f;
        void        Normalize();
        static void Reflect();
    };

    struct EditorCamera {
        Vector3 position{0, 0, 5};
        Vector3 pivot{0, 0, 0};
        float   yaw = 0, pitch = 0, orbitDistance = 5;
        float   verticalFov = .785398163f, nearPlane = .1f, farPlane = 10000.f;

        Vector3 Forward() const;
        Vector3 Right() const;
        Vector3 Up() const;
        Matrix4 View() const;
        Matrix4 Projection(float aspect) const;
        bool    Project(const Vector3 &point, Vector2 extent, Vector2 &screen) const;
        Vector3 Ray(Vector2 screen, Vector2 extent) const;
        float   WorldUnitsPerPixel(const Vector3 &point, float height) const;
        void    Look(float dx, float dy, float sensitivity, bool orbit);
        void    Pan(float dx, float dy, float height);
        void    Dolly(float distance);
        void    Fly(Vector3 input, float seconds, float speed);
        void    Focus(const Vector3 &target);
    };
} // namespace sky::editor
