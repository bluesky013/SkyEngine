#include <particle/ParticleEffect.h>

#include <algorithm>
#include <cmath>

namespace sky::particle {
    namespace {
        float Clamp01(float value)
        {
            return std::clamp(value, 0.f, 1.f);
        }

        Vector3 Lerp(const Vector3 &a, const Vector3 &b, float t)
        {
            return a + (b - a) * t;
        }

        Color Lerp(const Color &a, const Color &b, float t)
        {
            return Color{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
        }
    } // namespace

    bool IsValidAttribute(ParticleAttribute attribute) noexcept
    {
        return attribute < ParticleAttribute::END;
    }

    bool IsValidModuleType(ParticleModuleType type) noexcept
    {
        return type < ParticleModuleType::END;
    }

    bool IsValidSimTarget(ParticleSimTarget target) noexcept
    {
        return target < ParticleSimTarget::END;
    }

    bool IsValidRendererType(ParticleRendererType type) noexcept
    {
        return type < ParticleRendererType::END;
    }

    Vector3 ParticleCurve::Evaluate(float time) const
    {
        if (keys.empty())
            return {1.f, 1.f, 1.f};
        if (keys.size() == 1 || time <= keys.front().time)
            return keys.front().value;
        if (time >= keys.back().time)
            return keys.back().value;
        const auto  upper = std::upper_bound(keys.begin(), keys.end(), time, [](float value, const auto &key) { return value < key.time; });
        const auto &b     = *upper;
        const auto &a     = *(upper - 1);
        const auto  span  = b.time - a.time;
        return Lerp(a.value, b.value, span > 0.f ? (time - a.time) / span : 0.f);
    }

    Color ParticleGradient::Evaluate(float time) const
    {
        if (keys.empty())
            return {1.f, 1.f, 1.f, 1.f};
        if (keys.size() == 1 || time <= keys.front().time)
            return keys.front().color;
        if (time >= keys.back().time)
            return keys.back().color;
        const auto  upper = std::upper_bound(keys.begin(), keys.end(), time, [](float value, const auto &key) { return value < key.time; });
        const auto &b     = *upper;
        const auto &a     = *(upper - 1);
        const auto  span  = b.time - a.time;
        return Lerp(a.color, b.color, span > 0.f ? (time - a.time) / span : 0.f);
    }

    bool ParticleBounds::Valid() const
    {
        return std::isfinite(min.x) && std::isfinite(min.y) && std::isfinite(min.z) && std::isfinite(max.x) && std::isfinite(max.y) &&
               std::isfinite(max.z) && min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
} // namespace sky::particle
