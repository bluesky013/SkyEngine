#pragma once

#include <core/math/Vector2.h>
#include <cstdint>

namespace sky::ui {
    struct UIKeyEvent;
    struct UIPointerEvent;
    struct UIRect;
} // namespace sky::ui

namespace sky::editor {
    struct EditorCamera;

    class AssetPreviewCameraInput {
    public:
        bool Handle(EditorCamera &camera, const ui::UIPointerEvent &event, const ui::UIRect &image);
        bool Captured() const
        {
            return drag != Drag::None;
        }
        void Cancel()
        {
            drag = Drag::None;
        }
        static bool IsFrameKey(const ui::UIKeyEvent &event);

    private:
        enum class Drag { None, Orbit, Pan, Dolly };
        Drag     drag    = Drag::None;
        uint32_t pointer = 0, button = 0;
        Vector2  previous{};
    };
} // namespace sky::editor
