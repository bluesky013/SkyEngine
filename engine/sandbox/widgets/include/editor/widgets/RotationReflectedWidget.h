//
// Created on 2026/10/04.
//

#pragma once

#include <editor/widgets/ReflectedWidget.h>

namespace sky::editor {

    // Specialized widget for PropertyEditorKind::Rotation: a quaternion member is
    // presented and edited as Euler angles (YZX, degrees) by default, with a mode
    // toggle to edit raw quaternion x/y/z/w. Used by Transform.rotation, giving
    // the component a TRS editor.
    class RotationReflectedWidget : public ReflectedWidget {
    public:
        PropertyEditorKind Kind() const override
        {
            return PropertyEditorKind::Rotation;
        }

        void Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect) override;
        bool OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event, const sky::ui::UIRect &rect) override;

    private:
        struct Layout {
            sky::ui::UIRect body;
            sky::ui::UIRect button;
        };

        Layout          ComputeLayout(const ReflectedWidgetHost &host, const sky::ui::UIRect &control) const;
        sky::ui::UIRect Cell(const UiMetrics &metrics, const sky::ui::UIRect &control, int index, int count) const;

        int mode = 0; // 0 = Euler (YZX, degrees), 1 = Quaternion (x/y/z/w)
    };

} // namespace sky::editor
