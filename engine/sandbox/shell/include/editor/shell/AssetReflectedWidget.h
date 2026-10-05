//
// Created on 2026/10/04.
//

#pragma once

#include <editor/shell/ReflectedWidget.h>
#include <editor/core/asset/EditorAssetCatalog.h>

#include <vector>

namespace sky::editor {

    // Specialized widget for PropertyEditorKind::Asset: shows the asset name
    // (validated against the required type) with a browse button and a picker
    // popup listing assets of that type from the asset catalog.
    class AssetReflectedWidget : public ReflectedWidget {
    public:
        PropertyEditorKind Kind() const override { return PropertyEditorKind::Asset; }

        void Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field,
                   const sky::ui::UIRect &rect) override;
        bool OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event,
                    const sky::ui::UIRect &rect) override;

        bool HasPopup() const override { return openField != nullptr; }
        void PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context) override;
        bool OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;
        bool OnEscape(ReflectedWidgetHost &host) override;

    private:
        void Open(PropertyField &field, const sky::ui::UIRect &control, const sky::ui::UIRect &bounds);
        void Select(ReflectedWidgetHost &host, int index);
        static void FormatLabel(const PropertyField &field, std::string &outLabel, bool &outValid);

        PropertyField *openField = nullptr;
        int            hoverItem = -1;
        sky::ui::UIRect popupRect;
        std::vector<sky::ui::UIRect> itemRects;
        std::vector<EditorAssetItem> items;
    };

} // namespace sky::editor
