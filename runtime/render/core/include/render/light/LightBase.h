//
// Created by blues on 2024/6/10.
//

#pragma once

#include <core/math/MathUtil.h>

namespace sky {

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

    class Light {
    public:
        Light() = default;
        virtual ~Light() = default;

        virtual void Collect(LightInfo &info) = 0;

        void SetColor(const Vector4 &clr) { color = clr; }

    protected:
        Vector4 color;
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
