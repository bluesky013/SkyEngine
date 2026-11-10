//
// Created on 2026/11/10.
//

#pragma once

#include <curve/Curve.h>

#include <array>
#include <string_view>

namespace sky::curve::detail {
    template <typename Enum>
    struct EnumEntry {
        Enum        value;
        const char *token;
        const char *label;
    };

    inline constexpr std::array INTERPOLATIONS{
        EnumEntry{CurveInterpolation::LINEAR, "linear", "Linear"},
        EnumEntry{CurveInterpolation::STEP, "step", "Step"},
        EnumEntry{CurveInterpolation::CUBIC, "cubic", "Cubic"},
    };
    inline constexpr std::array WRAPS{
        EnumEntry{CurveWrap::CLAMP, "clamp", "Clamp"},
        EnumEntry{CurveWrap::REPEAT, "repeat", "Repeat"},
        EnumEntry{CurveWrap::MIRROR, "mirror", "Mirror"},
        EnumEntry{CurveWrap::LINEAR, "linear", "Linear"},
    };
    static_assert(INTERPOLATIONS.size() == static_cast<size_t>(CurveInterpolation::END));
    static_assert(WRAPS.size() == static_cast<size_t>(CurveWrap::END));

    template <typename Enum, size_t N>
    bool ParseEnum(std::string_view token, const std::array<EnumEntry<Enum>, N> &entries, Enum &out)
    {
        for (const auto &entry : entries) {
            if (token == entry.token) {
                out = entry.value;
                return true;
            }
        }
        return false;
    }

    template <typename Enum, size_t N>
    const char *EnumToken(Enum value, const std::array<EnumEntry<Enum>, N> &entries)
    {
        for (const auto &entry : entries) {
            if (value == entry.value)
                return entry.token;
        }
        return "";
    }
} // namespace sky::curve::detail
