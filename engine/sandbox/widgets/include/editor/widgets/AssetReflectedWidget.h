//
// Created on 2026/10/04.
//

#pragma once

#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/widgets/ReflectedWidget.h>

#include <vector>

namespace sky::editor {

    // References keep their UUID; paths and names are presentation-only catalog lookups.
    class AssetReflectedWidget : public ReflectedWidget {
    public:
        explicit AssetReflectedWidget(IEditorAssetCatalog *source = nullptr) : source(source)
        {
        }
        PropertyEditorKind Kind() const override
        {
            return PropertyEditorKind::Asset;
        }
        void ResetInteraction(ReflectedWidgetHost &host, ReflectedInteractionReset mode) override;

        void Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect) override;
        bool OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event, const sky::ui::UIRect &rect) override;
        bool QueryTooltip(
            const ReflectedWidgetHost &, const PropertyField &, const sky::ui::UIRect &, float, float, sky::ui::UITooltipTarget &) const override;
        bool BuildTooltip(const PropertyField &, uint64_t, std::string &) const override;
        bool QueryPopupTooltip(float, float, sky::ui::UITooltipTarget &) const override;
        bool BuildPopupTooltip(uint64_t, std::string &) const override;

        bool HasPopup() const override
        {
            return openField != nullptr;
        }
        void PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context) override;
        bool OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;
        bool OnEscape(ReflectedWidgetHost &host) override;

    private:
        void                 Open(PropertyField &field, const sky::ui::UIRect &control, const sky::ui::UIRect &bounds, const UiTheme &theme);
        void                 Select(ReflectedWidgetHost &host, int index);
        void                 FormatLabel(const PropertyField &field, std::string &outLabel, bool &outValid) const;
        bool                 FormatTooltip(const Uuid &, const std::string &expectedType, std::string &) const;
        IEditorAssetCatalog *source = nullptr;

        PropertyField               *openField       = nullptr;
        int                          hoverItem       = -1;
        size_t                       firstItem       = 0;
        uint32_t                     popupGeneration = 0;
        sky::ui::UIRect              popupRect;
        std::vector<sky::ui::UIRect> itemRects;
        std::vector<EditorAssetItem> items;
    };

} // namespace sky::editor
