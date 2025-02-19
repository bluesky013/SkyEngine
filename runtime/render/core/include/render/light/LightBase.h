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

        void Collect(LightInfo &info) override;
    private:
        Vector3 position;
        Vector3 direction;

        float range = 1.f;
    };

    class SpotLight : public Light {
    public:
        SpotLight() = default;
        ~SpotLight() override = default;

        void Collect(LightInfo &info) override;
    private:
        Vector3 position;
        Vector3 direction;

        float angle = 1.f;
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
