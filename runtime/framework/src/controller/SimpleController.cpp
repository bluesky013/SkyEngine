//
// Created by blues on 2024/5/19.
//

#include <framework/controller/SimpleController.h>
#include <framework/window/NativeWindow.h>
#include <algorithm>

namespace sky {

    FirstPersonController::FirstPersonController() = default;

    void FirstPersonController::SetMoveSpeed(float speed)
    {
        moveSpeed = speed;
    }

    Transform FirstPersonController::Resolve(float time, const Transform &trans)
    {
        Transform res = trans;

        auto euler = trans.rotation.ToEulerYZX();
        if (mouseButtons[MouseButtonType::RIGHT]) {
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

        if (keyButtons[ScanCode::KEY_W]) {
            res.translation += forward * time * moveSpeed;
        }
        if (keyButtons[ScanCode::KEY_S]) {
            res.translation -= forward * time * moveSpeed;
        }
        if (keyButtons[ScanCode::KEY_A]) {
            res.translation -= right * time * moveSpeed;
        }
        if (keyButtons[ScanCode::KEY_D]) {
            res.translation += right * time * moveSpeed;
        }

        return res;
    }

    void FirstPersonController::BindWindow(const NativeWindow *window_)
    {
        keyBinder.Bind(this);
        mouseBinder.Bind(this);

        window = window_;
    }

    bool FirstPersonController::FilterWindowID(WindowID id)
    {
        return window != nullptr && window->GetWinId() == id;
    }

    void FirstPersonController::OnMouseMotion(const MouseMotionEvent &event)
    {
        if (!FilterWindowID(event.winID)) {
            return;
        }

        currentX = event.x;
        currentY = event.y;
    }

    void FirstPersonController::OnMouseButtonDown(const MouseButtonEvent &event)
    {
        if (!FilterWindowID(event.winID)) {
            return;
        }

        mouseButtons[event.button] = true;

        if (event.button == MouseButtonType::RIGHT) {
            startX = currentX;
            startY = currentY;
        }
    }

    void FirstPersonController::OnMouseButtonUp(const MouseButtonEvent &event)
    {
        if (!FilterWindowID(event.winID)) {
            return;
        }

        mouseButtons[event.button] = false;
    }

    void FirstPersonController::OnMouseWheel(const MouseWheelEvent &event)
    {
        if (!FilterWindowID(event.winID)) {
            return;
        }

        moveSpeed += static_cast<float>(event.y);
        moveSpeed = std::clamp(moveSpeed, 0.1f, 100.f);
    }

    void FirstPersonController::OnKeyUp(const KeyboardEvent &event)
    {
        if (!FilterWindowID(event.winID)) {
            return;
        }

        keyButtons[event.scanCode] = false;
    }

    void FirstPersonController::OnKeyDown(const KeyboardEvent &event)
    {
        if (!FilterWindowID(event.winID)) {
            return;
        }

        keyButtons[event.scanCode] = true;
    }
} // namespace sky
