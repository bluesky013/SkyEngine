//
// Created by blues on 2024/5/19.
//

#pragma once

#include <core/math/Transform.h>
#include <core/event/Event.h>
#include <framework/window/IWindowEvent.h>
#include <unordered_map>

namespace sky {

    class FirstPersonController : public IWindowEvent {
    public:
        FirstPersonController();
        ~FirstPersonController() override = default;

        void BindWindow(const NativeWindow *window);
        Transform Resolve(float time, const Transform &trans);

    private:
        void OnMouseMove(int32_t x, int32_t y, int32_t rx, int32_t ry) override;
        void OnMouseButtonDown(MouseButtonType button) override;
        void OnMouseButtonUp(MouseButtonType button) override;
        void OnMouseWheel(int32_t wheelX, int32_t wheelY) override;
        void OnKeyUp(KeyButtonType) override;
        void OnKeyDown(KeyButtonType) override;

        EventBinder<IWindowEvent> binder;
        std::unordered_map<MouseButtonType, bool> mouseButtons;
        std::unordered_map<MouseButtonType, bool> keyButtons;
        int32_t startX = 0;
        int32_t startY = 0;

        int32_t currentX = 0;
        int32_t currentY = 0;

        float moveSpeed = 3.f;

        const NativeWindow *window = nullptr;
    };

} // namespace sky