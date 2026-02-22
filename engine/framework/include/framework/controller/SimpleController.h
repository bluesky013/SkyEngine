//
// Created by blues on 2024/5/19.
//

#pragma once

#include <core/math/Transform.h>
#include <core/event/Event.h>
#include <framework/window/IWindowEvent.h>
#include <unordered_map>

namespace sky {

    class FirstPersonController : public IMouseEvent, public IKeyboardEvent {
    public:
        FirstPersonController();
        ~FirstPersonController() override = default;

        void BindWindow(const NativeWindow *window);
        Transform Resolve(float time, const Transform &trans);

        void SetMoveSpeed(float speed);

    private:
        void OnMouseButtonDown(const MouseButtonEvent &event) override;
        void OnMouseButtonUp(const MouseButtonEvent &event) override;
        void OnMouseMotion(const MouseMotionEvent &event) override;
        void OnMouseWheel(const MouseWheelEvent &event) override;

        void OnKeyUp(const KeyboardEvent &event) override;
        void OnKeyDown(const KeyboardEvent &event) override;

        bool FilterWindowID(WindowID id);

        EventBinder<IKeyboardEvent> keyBinder;
        EventBinder<IMouseEvent> mouseBinder;

        std::unordered_map<MouseButtonType, bool> mouseButtons;
        std::unordered_map<ScanCode, bool> keyButtons;
        int32_t startX = 0;
        int32_t startY = 0;

        int32_t currentX = 0;
        int32_t currentY = 0;

        float moveSpeed = 3.f;

        const NativeWindow *window = nullptr;
    };

} // namespace sky
