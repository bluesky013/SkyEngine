//
// Created on 2026/10/22.
//

#pragma once

#include <editor/core/viewport/ViewportController.h>
#include <functional>
#include <memory>
#include <ui/widgets/Image.h>

namespace sky::editor {
    class SceneViewport final : public ui::Image {
    public:
        explicit SceneViewport(std::shared_ptr<ViewportController> controller);
        ~SceneViewport() override;
        const char                  *GetTypeName() const override;
        void                         OnPaint(ui::UIPaintContext &paint) override;
        ui::UIEventResult            OnPointerEvent(const ui::UIPointerEvent &event) override;
        ui::UIEventResult            OnKeyEvent(const ui::UIKeyEvent &event) override;
        void                         OnPointerCancel(uint32_t pointerId) override;
        void                         OnPointerButtonConflict(uint32_t pointerId) override;
        bool                         WantsPointerCapture() const override;
        std::function<void(Vector2)> extentChanged;
        std::function<bool()>        orbitHeld;

    protected:
        void ArrangeChildren(const ui::UIRect &content) override;

    private:
        void                                SyncPickScale();
        float                               pickScale = 0;
        GizmoStyle                          Style() const;
        std::shared_ptr<ViewportController> controller;
        ui::UIRect                          contentRect;
        Vector2                             clickStart;
        float                               clickThreshold = 0;
        bool                                clickExceeded  = false;
        GizmoHandle                         hover          = GizmoHandle::NONE;
        uint32_t                            pointerId = 0, button = 0;
    };
} // namespace sky::editor
