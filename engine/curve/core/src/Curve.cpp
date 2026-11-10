#include <curve/Curve.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace sky::curve {
    namespace {
        bool Finite(const Vector4 &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
        }
        bool Fail(std::string &error, const char *message)
        {
            error = message;
            return false;
        }
        float WrapTime(float time, CurveWrap wrap)
        {
            switch (wrap) {
            case CurveWrap::REPEAT: {
                const float wrapped = time - std::floor(time);
                return wrapped;
            }
            case CurveWrap::MIRROR: {
                float wrapped = std::fmod(time, 2.f);
                if (wrapped < 0.f)
                    wrapped += 2.f;
                return wrapped <= 1.f ? wrapped : 2.f - wrapped;
            }
            case CurveWrap::LINEAR:
            case CurveWrap::CLAMP:
            default: return std::clamp(time, 0.f, 1.f);
            }
        }
        Vector4 Extrapolate(const std::vector<CurveKey> &keys, float time, bool after)
        {
            if (keys.empty())
                return {};
            if (keys.size() == 1)
                return keys.front().value;
            const auto  &a    = after ? keys[keys.size() - 2] : keys[0];
            const auto  &b    = after ? keys.back() : keys[1];
            const double span = static_cast<double>(b.time) - a.time;
            if (span <= 0.0)
                return after ? keys.back().value : keys.front().value;
            const auto  &anchor = after ? keys.back() : keys.front();
            const double alpha  = (static_cast<double>(time) - anchor.time) / span;
            Vector4      result;
            for (uint32_t i = 0; i < 4; ++i)
                result.v[i] = static_cast<float>(anchor.value.v[i] + (static_cast<double>(b.value.v[i]) - a.value.v[i]) * alpha);
            return result;
        }
        Vector4 Interpolate(const Vector4 &a, const Vector4 &b, double alpha)
        {
            Vector4 result;
            for (uint32_t i = 0; i < 4; ++i)
                result.v[i] = static_cast<float>(std::lerp(static_cast<double>(a.v[i]), static_cast<double>(b.v[i]), alpha));
            return result;
        }
        Vector4 Hermite(const Vector4 &a, const Vector4 &b, const Vector4 &outTangent, const Vector4 &inTangent, double t, double span)
        {
            const double t2 = t * t, t3 = t2 * t;
            const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
            const double h10 = t3 - 2.0 * t2 + t;
            const double h01 = -2.0 * t3 + 3.0 * t2;
            const double h11 = t3 - t2;
            Vector4      result;
            for (uint32_t i = 0; i < 4; ++i)
                result.v[i] = static_cast<float>(a.v[i] * h00 + outTangent.v[i] * (span * h10) + b.v[i] * h01 + inTangent.v[i] * (span * h11));
            return result;
        }
    } // namespace

    bool Curve::Validate(std::string &error) const
    {
        error.clear();
        if (channels < 1 || channels > 4)
            return Fail(error, "Curve channels must be in [1, 4]");
        if (interpolation >= CurveInterpolation::END)
            return Fail(error, "Curve interpolation is invalid");
        if (preWrap >= CurveWrap::END || postWrap >= CurveWrap::END)
            return Fail(error, "Curve wrap mode is invalid");
        if (keys.empty())
            return Fail(error, "Curve requires at least one key");
        if (keys.size() > MAX_CURVE_KEYS)
            return Fail(error, "Curve has too many keys");
        float previous = -1.f;
        for (const auto &key : keys) {
            if (!std::isfinite(key.time) || key.time < 0.f || key.time > 1.f || key.time <= previous)
                return Fail(error, "Curve keys must be finite and strictly ordered in [0, 1]");
            if (!Finite(key.value) || !Finite(key.tangentIn) || !Finite(key.tangentOut))
                return Fail(error, "Curve key value or tangent is invalid");
            previous = key.time;
        }
        return true;
    }

    Vector4 Curve::Evaluate(float time) const
    {
        if (keys.empty())
            return {};
        Vector4 result{0.f, 0.f, 0.f, 0.f};
        if (time < 0.f && preWrap == CurveWrap::LINEAR) {
            result = Extrapolate(keys, time, false);
        } else if (time > 1.f && postWrap == CurveWrap::LINEAR) {
            result = Extrapolate(keys, time, true);
        } else {
            const float sample = WrapTime(time, time < 0.f ? preWrap : time > 1.f ? postWrap : CurveWrap::CLAMP);
            const auto  upper = std::lower_bound(keys.begin(), keys.end(), sample, [](const CurveKey &key, float value) { return key.time < value; });
            if (upper == keys.begin()) {
                result = keys.front().value;
            } else if (upper == keys.end()) {
                result = keys.back().value;
            } else if (interpolation == CurveInterpolation::STEP) {
                result = upper->time == sample ? upper->value : std::prev(upper)->value;
            } else {
                const auto  &a     = *std::prev(upper);
                const auto  &b     = *upper;
                const double span  = static_cast<double>(b.time) - a.time;
                const double alpha = span > 0.0 ? (static_cast<double>(sample) - a.time) / span : 0.0;
                if (interpolation == CurveInterpolation::CUBIC)
                    result = Hermite(a.value, b.value, a.tangentOut, b.tangentIn, alpha, span);
                else
                    result = Interpolate(a.value, b.value, alpha);
            }
        }
        for (uint32_t i = channels; i < 4; ++i)
            result.v[i] = 0.f;
        return result;
    }
} // namespace sky::curve
