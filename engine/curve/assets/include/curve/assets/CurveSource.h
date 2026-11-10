#pragma once

#include <curve/Curve.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace sky::curve {

    inline constexpr size_t MAX_CURVE_SOURCE_BYTES = 256u * 1024u;

    bool ParseCurveSource(std::string_view text, Curve &curve, std::string &error);
    bool WriteCurveSource(const Curve &curve, std::string &text);

} // namespace sky::curve
