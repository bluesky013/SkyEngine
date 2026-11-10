#pragma once

#include <core/math/Vector4.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::curve {

    inline constexpr uint32_t MAX_CURVE_KEYS = 256;

    struct CurveKey {
        float   time = 0.f;
        Vector4 value{0.f, 0.f, 0.f, 0.f};
        // Hermite slopes in value/time; used when interpolation is CUBIC.
        Vector4 tangentIn{0.f, 0.f, 0.f, 0.f};
        Vector4 tangentOut{0.f, 0.f, 0.f, 0.f};
    };

    enum class CurveInterpolation : uint8_t { LINEAR, STEP, CUBIC, END };
    // Extrapolation for samples before the first / after the last key.
    enum class CurveWrap : uint8_t { CLAMP, REPEAT, MIRROR, LINEAR, END };

    struct Curve {
        uint32_t           channels      = 1;
        CurveInterpolation interpolation = CurveInterpolation::LINEAR;
        CurveWrap          preWrap       = CurveWrap::CLAMP;
        CurveWrap          postWrap      = CurveWrap::CLAMP;
        std::vector<CurveKey> keys;

        bool    Validate(std::string &error) const;
        Vector4 Evaluate(float time) const;
    };

} // namespace sky::curve
