//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIRect.h>

#include <cmath>

namespace sky::ui {

    struct UITransform {
        float translationX = 0.0f;
        float translationY = 0.0f;
        float rotation = 0.0f; // radians
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float pivotX = 0.0f;
        float pivotY = 0.0f;

        bool IsIdentity() const
        {
            return translationX == 0.0f && translationY == 0.0f &&
                   rotation == 0.0f && scaleX == 1.0f && scaleY == 1.0f;
        }

        // T(translation) * T(pivot) * R * S * T(-pivot)
        UI2DTransform ToMatrix() const
        {
            const float c = std::cos(rotation);
            const float s = std::sin(rotation);

            UI2DTransform matrix;
            matrix.m00 = c * scaleX;
            matrix.m01 = -s * scaleY;
            matrix.m02 = translationX + pivotX - (c * scaleX * pivotX - s * scaleY * pivotY);
            matrix.m10 = s * scaleX;
            matrix.m11 = c * scaleY;
            matrix.m12 = translationY + pivotY - (s * scaleX * pivotX + c * scaleY * pivotY);
            return matrix;
        }
    };

} // namespace sky::ui
