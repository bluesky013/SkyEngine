//
// Created by blues on 2024/5/19.
//

#include <framework/controller/SimpleController.h>
#include <framework/window/NativeWindow.h>
#include <algorithm>

namespace sky {

    FirstPersonController::FirstPersonController() = default;

    Transform FirstPersonController::Resolve(float time, const Transform &trans)
    {
        Transform res = trans;

        auto euler = trans.rotation.ToEulerYZX();
        if (mouseButtons[MouseButton::MOUSE_BUTTON_RIGHT]) {
            float diffX = static_cast<float>(currentX - startX) / static_cast<float>(window->GetWidth());
            float diffY = static_cast<float>(currentY - startY) / static_cast<float>(window->GetHeight());

            startX = currentX;
            startY = currentY;

            euler.y -= diffX * 20000.0f * time;
            euler.x -= diffY * 20000.0f * time;
            res.rotation.FromEulerYZX(euler);
        }

        auto forward = trans.rotation * (-VEC3_Z);
        auto up = trans.rotation * (VEC3_Y);
        auto right = forward.Cross(up);

        if (keyButtons[KeyButton::KEY_UP] || keyButtons[KeyButton::KEY_W]) {
            res.translation += forward * time * moveSpeed;
        }
        if (keyButtons[KeyButton::KEY_DOWN] || keyButtons[KeyButton::KEY_S]) {
            res.translation -= forward * time * moveSpeed;
        }
        if (keyButtons[KeyButton::KEY_LEFT] || keyButtons[KeyButton::KEY_A]) {
            res.translation -= right * time * moveSpeed;
        }
        if (keyButtons[KeyButton::KEY_RIGHT] || keyButtons[KeyButton::KEY_D]) {
            res.translation += right * time * moveSpeed;
        }

        return res;
    }

    void FirstPersonController::BindWindow(const NativeWindow *window_)
    {
        binder.Bind(this, window_);
        window = window_;
    }

    void FirstPersonController::OnMouseMove(int32_t x, int32_t y, int32_t relX, int32_t relY)
    {
        if (window == nullptr) {
            return;
        }

        currentX = x;
        currentY = y;
    }

    void FirstPersonController::OnMouseButtonDown(MouseButtonType button)
    {
        mouseButtons[button] = true;

        if (button == MouseButton::MOUSE_BUTTON_RIGHT) {
            startX = currentX;
            startY = currentY;
        }
    }

    void FirstPersonController::OnMouseButtonUp(MouseButtonType button)
    {
        mouseButtons[button] = false;
    }

    void FirstPersonController::OnMouseWheel(int32_t wheelX, int32_t wheelY)
    {
        moveSpeed += static_cast<float>(wheelY);
        moveSpeed = std::clamp(moveSpeed, 0.1f, 100.f);
    }

    void FirstPersonController::OnKeyUp(KeyButtonType button)
    {
        keyButtons[button] = false;
    }

    void FirstPersonController::OnKeyDown(KeyButtonType button)
    {
        keyButtons[button] = true;
    }
} // namespace sky