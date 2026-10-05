//
// Created on 2026/10/04.
//

#pragma once

#include <editor/shell/ColorPicker.h>
#include <editor/shell/ReflectedWidget.h>

namespace sky::editor {

    // Specialized widget for PropertyEditorKind::Color. Works purely through
    // reflection (channels located by float-member name r/g/b/a) plus ColorPicker;
    // it does not reference any engine color type.
    class ColorReflectedWidget : public ReflectedWidget {
    public:
        PropertyEditorKind Kind() const override { return PropertyEditorKind::Color; }

        void Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field,
                   const sky::ui::UIRect &rect) override;
        bool OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event,
                    const sky::ui::UIRect &rect) override;
        bool OnMove(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;
        bool OnUp(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;

        bool HasPopup() const override { return picker.IsOpen(); }
        void PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context) override;
        bool OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;
        bool OnEscape(ReflectedWidgetHost &host) override;

    private:
        sky::ui::UIRect Segment(const sky::ui::UIRect &control, int index) const;
        sky::ui::UIRect Slider(const sky::ui::UIRect &segment) const;
        float Fraction(float x, float left, float right) const;

        ColorPicker     picker;
        PropertyField  *target = nullptr;
        sky::ui::UIRect dragControl;
        int             dragComponent = -1;
        double          dragValue = 0.0;
    };

} // namespace sky::editor
