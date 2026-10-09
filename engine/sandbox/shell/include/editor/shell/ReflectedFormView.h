//
// Created on 2026/10/04.
//

#pragma once

#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/property/PropertyEditor.h>
#include <editor/core/property/ReflectedForm.h>
#include <editor/shell/ReflectedWidget.h>
#include <editor/shell/UiSkin.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIDrawData.h>
#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIPaintContext.h>
#include <ui/UIRect.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sky::editor {

    class CommandService;
    class PropertyEditorRegistry;

} // namespace sky::editor

namespace sky::ui {
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Reusable, theme-driven view that renders and edits a ReflectedForm:
    // scalars, bools, enum dropdowns, sliders, strings, colors (with a
    // Blender-style picker), vectors, asset references, nested structs and
    // sequences. All edits go through CommandService (undoable) and honor
    // per-member attributes, type validation and reset-to-default.
    class ReflectedFormView : public sky::ui::UIElement, public ReflectedWidgetHost {
    public:
        ReflectedFormView(sky::ui::UITextSystem *text, std::string title);
        ~ReflectedFormView() override;

        // Binds the view to a reflected data object (rebuilds the form). The reset
        // baseline defaults to the type's default value; pass `baseline` to make
        // reset restore a different baseline (see ReflectedForm::Build).
        void Bind(const PropertyObject &object, const PropertyObject *baseline = nullptr);
        void Refresh();

        // Hides the built-in header (title + "ctrl+Z/Y undo" hint). Useful when the form is embedded
        // as a section of a larger panel that already has its own title.
        void SetHeaderVisible(bool visible)
        {
            headerVisible = visible;
            MarkPaintDirty();
        }

        // Notifies the owner after any committed edit to the bound data (used by
        // the world-config panel to mark its document dirty).
        void SetOnEdited(std::function<void()> callback)
        {
            form.SetOnChanged(std::move(callback));
        }

        // ReflectedWidgetHost
        ReflectedForm &Form() override
        {
            return form;
        }
        CommandService &Commands() override;
        const UiSkin   &Skin() const override
        {
            return skin;
        }
        sky::ui::UITextSystem *Text() const override
        {
            return textSystem;
        }
        sky::ui::UIRect ViewBounds() const override
        {
            // Bounds used to place popups (enum dropdowns): the top-level element, so a popup only flips up
            // near the window edge, not the (possibly small) form edge.
            const sky::ui::UIElement *root = this;
            while (root->GetParent() != nullptr) {
                root = root->GetParent();
            }
            return root->GetBounds();
        }

        // Natural height that fits all rows without an internal scrollbar (for stacking hosts).
        float GetPreferredHeight();
        void  MarkDirty() override;
        void  RefreshForm() override;
        bool  IsHovered(const PropertyField &field) const override
        {
            return hoverField == &field;
        }
        void BeginTextEdit(PropertyField                           &field,
                           int                                      component,
                           PropertyEditorKind                       inputKind,
                           const std::string                       &initial,
                           std::function<bool(const std::string &)> commit) override;

        const char *GetTypeName() const override
        {
            return "ReflectedFormView";
        }
        // True while a popup (enum dropdown / color picker) is open: the host should deliver the pointer
        // to this form regardless of hit-testing, since the popup may extend beyond its bounds.
        bool WantsPointerCapture() const override;

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;
        void                   OnPointerLeave(const sky::ui::UIPointerEvent &event) override;

    protected:
        // Hooks for specialized panels (e.g. a demo with a Live toggle).
        virtual void OnViewTick()
        {
        }
        virtual float ExtraHeaderWidth() const
        {
            return 0.0f;
        }
        virtual void PaintExtraHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect)
        {
            (void)context;
            (void)rect;
        }
        virtual sky::ui::UIEventResult HandleExtraHeaderPointer(const sky::ui::UIPointerEvent &event)
        {
            (void)event;
            return sky::ui::UIEventResult::UNHANDLED;
        }

        const UiTheme &Theme() const
        {
            return skin.Theme();
        }
        const sky::ui::UIRect &HeaderExtraRect() const
        {
            return headerExtraRect;
        }

    private:
        struct Row {
            PropertyField  *field = nullptr;
            sky::ui::UIRect rowRect;
            sky::ui::UIRect labelRect;
            sky::ui::UIRect controlRect;
            sky::ui::UIRect revertRect;
            int             depth = 0;
            bool            alt   = false;
        };

        void       PollExternalChanges();
        void       BuildRows();
        void       LayoutField(PropertyField &field, int depth, float &y, bool &alt);
        const Row *RowAt(float x, float y) const;
        const Row *RowOf(const PropertyField *field) const;

        sky::ui::UIRect SliderRect(const sky::ui::UIRect &control) const;
        sky::ui::UIRect VectorCell(const sky::ui::UIRect &control, int index, int count) const;

        void ComputeHeaderRects();
        void DrawHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &bounds);
        void DrawSection(sky::ui::UIPaintContext &context, FormSection &section, float &y);
        void DrawField(sky::ui::UIPaintContext &context, PropertyField &field, float &y);
        void DrawRevertIcon(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, bool hovered);
        void DrawContainer(sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect);
        void DrawScalar(sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect);
        void DrawEditBox(sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect);
        int  WidgetRowCount(const PropertyField &field) const;

        void BeginInteraction(const Row &row, float x, float y);
        void HandleSequenceButton(PropertyField &field, const sky::ui::UIRect &rect, float x);

        // Generic float-component access over a reflected struct (works for any
        // vector-like type, not just Vector2/3/4).
        void CommitEdit();
        void CancelEdit();

        sky::ui::UITextSystem  *textSystem = nullptr;
        std::string             title;
        CommandService         *commands = nullptr;
        PropertyEditorRegistry *registry = nullptr;
        UiSkin                  skin;

        ReflectedForm form;

        std::vector<Row> rows;
        float            layoutLeft    = 0.0f;
        float            layoutRight   = 0.0f;
        float            labelWidth    = 0.0f;
        float            scroll        = 0.0f;
        float            contentHeight = 0.0f;
        bool             headerVisible = true;

        // Generic scrollbar interaction (hover + drag), shared by every reflected form.
        sky::ui::UIRect ContentRect() const;
        bool            scrollDrag       = false;
        bool            hoverScroll      = false;
        float           scrollDragOffset = 0.0f;

        sky::ui::UIRect headerTitleRect;
        sky::ui::UIRect hintRect;
        sky::ui::UIRect headerExtraRect;

        ReflectedWidget *activeWidget = nullptr;

        uint64_t lastRevision           = 0;
        bool     pendingExternalRefresh = false;

        PropertyField *hoverField = nullptr;

        PropertyField                           *editField     = nullptr;
        int                                      editComponent = -1;
        PropertyEditorKind                       editKind      = PropertyEditorKind::String;
        std::string                              editText;
        size_t                                   editCaret   = 0;
        bool                                     editInvalid = false;
        std::function<bool(const std::string &)> editCommit;

        PropertyField *dragField     = nullptr;
        int            dragComponent = -1;
        double         dragValue     = 0.0;
    };

} // namespace sky::editor
