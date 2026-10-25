// Generated material keys bind typed models to reflected forms. Save validates the schema before writing/cooking.

#pragma once

#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/resource/Material.h>
#include <core/util/Uuid.h>
#include <editor/widgets/UiSkin.h>
#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sky {
    struct TypeNode;
}

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
    class GridPanel;
} // namespace sky::ui

namespace sky::editor {
    class ReflectedFormView;
    class CommandService;
} // namespace sky::editor

namespace sky::aurora::editor {

    // Row kinds: a regular editable field, a technique group header (with delete), or the add button.
    enum class MaterialRowKind : uint8_t {
        Field = 0,
        TechniqueHeader,
        AddTechnique,
        Diagnostic,
    };

    // One editable row: a typed value, a texture Uuid, or a technique string field.
    struct MaterialEditorRow {
        std::string           name;
        MaterialPropertyType  type = MaterialPropertyType::FLOAT;
        MaterialPropertyValue value;

        // String rows (technique shader path / tag / render-state text).
        std::string text;
        bool        isString       = false;
        bool        overridden     = true;
        int         techniqueIndex = -1; // >= 0 -> persist to techniques[techniqueIndex]
        int         techniqueField = 0;  // 0 = tag, 1 = shader, 2 = entry, 10 = renderState
        std::string renderKey;           // renderState JSON key (nested with '.') when techniqueField == 10

        MaterialRowKind kind = MaterialRowKind::Field;
    };

    class MaterialEditorView : public sky::ui::UIElement {
    public:
        MaterialEditorView(sky::ui::UITextSystem       *textSystem,
                           const Uuid                  &asset,
                           std::string                  title,
                           bool                         isInstance,
                           bool                         enableTechniques = false,
                           sky::editor::CommandService *history          = nullptr);
        ~MaterialEditorView() override;

        const char *GetTypeName() const override
        {
            return "MaterialEditorView";
        }

        void                   Measure(float &outWidth, float &outHeight) override;
        void                   ArrangeChildren(const sky::ui::UIRect &content) override;
        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;
        bool                   WantsPointerCapture() const override;

        // Asset-view Save hook (called by the asset-view base window): writes the buffered edits
        // to the material source and re-cooks. Returns false when there is no source / nothing to do.
        bool               Save();
        void               Tick();
        bool               IsEditing() const;
        void               CancelInteractions();
        const std::string &Error() const
        {
            return schemaError;
        }
        bool HasPendingEdits() const
        {
            return dirty;
        }

    private:
        struct PickerItem {
            Uuid        uuid;
            std::string name;
        };

        void                        RebuildRowSlots();
        const sky::editor::UiTheme &Theme() const
        {
            return skin.Theme();
        }
        float           LabelWidth() const;
        sky::ui::UIRect PickerNoneRect() const;
        sky::ui::UIRect RowRect(size_t index) const;
        void            CommitValue(int index);
        void            CommitTexture(int index, const Uuid &id);
        bool            PersistProperty(int index);
        void            OpenPicker(int index, const sky::ui::UIRect &row);
        void            ReloadTechniques();
        void            ClearPropertyForms();
        void            RebuildPropertyForms();
        void            AddTechnique();
        void            DeleteTechnique(int techniqueIndex);

        sky::ui::UITextSystem                        *text = nullptr;
        sky::editor::UiSkin                           skin;
        Uuid                                          asset;
        std::string                                   title;
        bool                                          isInstance = false;
        MaterialAssetData                             materialSource;      // reflected `.mat` source (material)
        MaterialInstanceSource                        instanceSource;      // reflected `.mati` source (instance)
        std::vector<MaterialEditorRow>                rows;                // techniques (grouped) + properties
        sky::ui::GridPanel                           *rowsPanel = nullptr; // owns the themed row layout
        std::vector<sky::editor::ReflectedFormView *> propertyForms;
        std::shared_ptr<void>                         propertyGeneration = std::make_shared<int>(0);
        std::vector<MaterialPropertyData>             formValues;
        std::vector<std::unique_ptr<sky::TypeNode>>   formNodes;
        MaterialSchema                                generatedSchema;
        std::string                                   schemaError;
        bool                                          allowTechniques = false;
        bool                                          dirty           = false;
        bool                                          reloadPending   = true;
        sky::editor::CommandService                  *history         = nullptr;

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
    void RegisterMaterialEditorProvider(std::shared_ptr<const void> lease = {});
    void UnregisterMaterialEditorProvider();

} // namespace sky::aurora::editor
