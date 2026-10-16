//
// Created on 2026/09/19.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::ui {

    struct UIPoint {
        float x = 0.0f;
        float y = 0.0f;
    };

    // Affine 2x3 transform: p' = M * p (m02/m12 are translation).
    struct UI2DTransform {
        float m00 = 1.0f;
        float m01 = 0.0f;
        float m02 = 0.0f;
        float m10 = 0.0f;
        float m11 = 1.0f;
        float m12 = 0.0f;

        static UI2DTransform Identity() { return UI2DTransform{}; }

        UI2DTransform operator*(const UI2DTransform &rhs) const
        {
            UI2DTransform result;
            result.m00 = m00 * rhs.m00 + m01 * rhs.m10;
            result.m01 = m00 * rhs.m01 + m01 * rhs.m11;
            result.m02 = m00 * rhs.m02 + m01 * rhs.m12 + m02;
            result.m10 = m10 * rhs.m00 + m11 * rhs.m10;
            result.m11 = m10 * rhs.m01 + m11 * rhs.m11;
            result.m12 = m10 * rhs.m02 + m11 * rhs.m12 + m12;
            return result;
        }

        // Use double precision for the determinant: a small but invertible
        // scale must not be rejected by an absolute float epsilon. Failure
        // leaves out unchanged, including when the inverse cannot fit in floats.
        bool TryInverse(UI2DTransform &out) const
        {
            if (!std::isfinite(m00) || !std::isfinite(m01) || !std::isfinite(m02) ||
                !std::isfinite(m10) || !std::isfinite(m11) || !std::isfinite(m12)) {
                return false;
            }

            const double determinant = static_cast<double>(m00) * m11 - static_cast<double>(m01) * m10;
            if (determinant == 0.0 || !std::isfinite(determinant)) {
                return false;
            }
            const double values[] = {
                m11 / determinant,
                -static_cast<double>(m01) / determinant,
                (static_cast<double>(m01) * m12 - static_cast<double>(m11) * m02) / determinant,
                -static_cast<double>(m10) / determinant,
                m00 / determinant,
                (static_cast<double>(m10) * m02 - static_cast<double>(m00) * m12) / determinant,
            };
            constexpr double limit = std::numeric_limits<float>::max();
            for (double value : values) {
                if (!std::isfinite(value) || value < -limit || value > limit) {
                    return false;
                }
            }

            UI2DTransform inverse;
            inverse.m00 = static_cast<float>(values[0]);
            inverse.m01 = static_cast<float>(values[1]);
            inverse.m02 = static_cast<float>(values[2]);
            inverse.m10 = static_cast<float>(values[3]);
            inverse.m11 = static_cast<float>(values[4]);
            inverse.m12 = static_cast<float>(values[5]);
            out = inverse;
            return true;
        }

        void Apply(float &x, float &y) const
        {
            const float tx = m00 * x + m01 * y + m02;
            const float ty = m10 * x + m11 * y + m12;
            x = tx;
            y = ty;
        }
    };

    struct UIRect {
        float left   = 0.0f;
        float top    = 0.0f;
        float right  = 0.0f;
        float bottom = 0.0f;

        float Width() const { return right - left; }
        float Height() const { return bottom - top; }
        bool IsEmpty() const { return right <= left || bottom <= top; }
        bool Contains(float x, float y) const { return x >= left && x < right && y >= top && y < bottom; }

        static UIRect Intersect(const UIRect &a, const UIRect &b)
        {
            UIRect result;
            result.left   = std::max(a.left, b.left);
            result.top    = std::max(a.top, b.top);
            result.right  = std::min(a.right, b.right);
            result.bottom = std::min(a.bottom, b.bottom);
            return result;
        }
    };

} // namespace sky::ui
