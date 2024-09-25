//
// Created by blues on 2024/6/10.
//

#pragma once

#include <core/math/MathUtil.h>

namespace sky {

    enum class LightType : uint32_t {
        DIRECT,
        POINT,
        SPOT
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

    class DirectLight : public Light {
    public:
        DirectLight() = default;
        ~DirectLight() override = default;

        void SetDirection(const Vector3 &dir) { direction = Cast(dir); }

        void Collect(LightInfo &info) override;
    private:
        Vector4 direction;
    };

} // namespace sky
