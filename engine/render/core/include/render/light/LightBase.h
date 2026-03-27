//
// Created by blues on 2024/6/10.
//

#pragma once

#include <core/math/MathUtil.h>

namespace sky {
    class SceneView;

    enum class LightType : uint32_t {
        POINT   = 0,
        DIRECT  = 1,
        SPOT    = 2
    };

    struct LightInfo {
        Vector4 position;
        Vector4 direction;
        Vector4 color;
        Vector4 rsv;
    };

    class MainDirectLight {
    public:
        MainDirectLight() = default;
        ~MainDirectLight() = default;

        void SetColor(const ColorRGB &clr)
        {
            color[0] = clr.r;
            color[1] = clr.g;
            color[2] = clr.b;
        }

        void SetIntensity(float intensity)
        {
            color[3] = intensity;
        }

        void SetDirection(const Vector3 &dir)
        {
            direction = dir;
        }

        void SetWorldMatrix(const Matrix4 &mtx)
        {
            worldMatrix = mtx;
        }

        void SetCastShadow(bool shadow)
        {
            castShadow = shadow;
        }

        const Vector4 &GetColor() const { return color; }
        const Vector3 &GetDirection() const { return direction; }

        void BuildMatrix(SceneView& view);
        const Matrix4& GetMatrix() const { return viewProject; }

    private:
        Vector4 color; // [0-2]: color rgb [3] color intensity
        Vector3 direction;
        Matrix4 worldMatrix;
        Matrix4 viewProject;

        bool castShadow = false;
    };

    class Light {
    public:
        Light() = default;
        virtual ~Light() = default;

        virtual void Collect(LightInfo &info) = 0;

        void SetColor(const ColorRGB &clr)
        {
            color[0] = clr.r;
            color[1] = clr.g;
            color[2] = clr.b;
        }

        void SetIntensity(float intensity)
        {
            color[3] = intensity;
        }

    protected:
        Vector4 color; // [0-2]: color rgb [3] color intensity
    };

    class PointLight : public Light {
    public:
        PointLight() = default;
        ~PointLight() override = default;

        void SetPosition(const Vector3 &pos) { position = pos; }
        void SetRange(float r) { range = r; }

        const Vector3 &GetPosition() const { return position; }
        float GetRange() const { return range; }

        void Collect(LightInfo &info) override;
    private:
        Vector3 position;
        float range = 10.f;
    };

    class SpotLight : public Light {
    public:
        SpotLight() = default;
        ~SpotLight() override = default;

        void SetPosition(const Vector3 &pos) { position = pos; }
        void SetDirection(const Vector3 &dir) { direction = dir; }
        void SetRange(float r) { range = r; }
        void SetInnerAngle(float radians) { innerAngle = radians; }
        void SetOuterAngle(float radians) { outerAngle = radians; }

        const Vector3 &GetPosition() const { return position; }
        const Vector3 &GetDirection() const { return direction; }
        float GetRange() const { return range; }
        float GetInnerAngle() const { return innerAngle; }
        float GetOuterAngle() const { return outerAngle; }

        void Collect(LightInfo &info) override;
    private:
        Vector3 position;
        Vector3 direction = Vector3(0.f, -1.f, 0.f);

        float range = 10.f;
        float innerAngle = 0.436f; // ~25 degrees
        float outerAngle = 0.524f; // ~30 degrees
    };

    class DirectLight : public Light {
    public:
        DirectLight() = default;
        ~DirectLight() override = default;

        void SetDirection(const Vector3 &dir) { direction = dir; }

        void Collect(LightInfo &info) override;
    private:
        Vector3 direction;
    };

} // namespace sky
