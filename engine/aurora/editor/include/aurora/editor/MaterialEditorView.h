//
// Editable material / material-instance property list, surfaced as custom asset-view content
// (see asset-viewer-widget). Dispatches per MaterialPropertyType directly (no reflection): value
// rows edit the uniform bytes, texture rows open an AuroraTexture picker. Commits write the source
// JSON ("properties" object) and re-cook.
//

#pragma once

#include <aurora/resource/Material.h>
#include <core/util/Uuid.h>
#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
    class StackPanel;
} // namespace sky::ui

namespace sky::aurora::editor {

    // Row kinds: a regular editable field, a technique group header (with delete), or the add button.
    enum class MaterialRowKind : uint8_t {
        Field = 0,
        TechniqueHeader,
        AddTechnique,
    };

    // One editable row: a typed value, a texture Uuid, or a technique string field.
    struct MaterialEditorRow {
        std::string          name;
        MaterialPropertyType type = MaterialPropertyType::FLOAT;
        std::vector<uint8_t> value;
        Uuid                 texture;

        // String rows (technique shader path / tag / render-state text).
        std::string text;
        bool        isString       = false;
        int         techniqueIndex = -1; // >= 0 -> persist to techniques[techniqueIndex]
        int         techniqueField = 0;  // 0 = tag, 1 = shader, 2 = entry, 10 = renderState
        std::string renderKey;           // renderState JSON key (nested with '.') when techniqueField == 10

        MaterialRowKind kind = MaterialRowKind::Field;
    };

    class MaterialEditorView : public sky::ui::UIElement {
    public:
        MaterialEditorView(sky::ui::UITextSystem         *textSystem,
                           const Uuid                    &asset,
                           std::string                    title,
                           std::vector<MaterialEditorRow> propertyRows,
                           bool                           enableTechniques = false);
        ~MaterialEditorView() override;

        const char *GetTypeName() const override
        {
            return "MaterialEditorView";
        }

        void                   Measure(float &outWidth, float &outHeight) override;
        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;
        bool                   WantsPointerCapture() const override;

        // Asset-view Save hook (called by the asset-view base window): writes the buffered edits
        // to the material source and re-cooks. Returns false when there is no source / nothing to do.
        bool Save();
        bool HasPendingEdits() const
        {
            return dirty;
        }

    private:
        struct PickerItem {
            Uuid        uuid;
            std::string name;
        };

        void            RebuildRowSlots();
        void            LayoutRows();
        sky::ui::UIRect RowRect(size_t index) const;
        void            CommitValue(int index);
        void            CommitTexture(int index, const Uuid &id);
        void            OpenPicker(int index, const sky::ui::UIRect &row);
        void            ReloadTechniques();
        void            AddTechnique();
        void            DeleteTechnique(int techniqueIndex);

        sky::ui::UITextSystem         *text = nullptr;
        Uuid                           asset;
        std::string                    title;
        std::vector<MaterialEditorRow> propertyRows;              // cached property rows
        std::vector<MaterialEditorRow> rows;                      // techniques (grouped) + properties
        sky::ui::StackPanel           *rowsPanel       = nullptr; // owns the vertical row layout
        bool                           allowTechniques = false;
        bool                           dirty           = false;

        int         editingRow = -1;
        std::string editText;
        int         hoverRow = -1;

        int                          pickerRow = -1;
        std::vector<PickerItem>      pickerItems;
        std::vector<sky::ui::UIRect> pickerRects;
        sky::ui::UIRect              pickerRect;
        int                          pickerHover = -1;
    };

    // Registers the material / material-instance providers (AssetViewProviderRegistry).
    void RegisterMaterialEditorProvider();

} // namespace sky::aurora::editor
