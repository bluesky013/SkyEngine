#pragma once

#include <curve/Curve.h>
#include <editor/core/command/CommandService.h>
#include <ui/UIElement.h>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sky::ui {
    class UITextSystem;
}
namespace sky::editor {
    class ReflectedFormView;

    enum class CurveEditorCommand : uint32_t {
        AddKey,
        RemoveSelected,
        Duplicate,
        Copy,
        Paste,
        FrameAll,
        ToggleSnap,
        SetLinear,
        SetStep,
        SetCubic,
        END
    };

    struct CurveEditorCommandInfo {
        CurveEditorCommand command;
        const char        *id;
        const char        *label;
        const char        *icon;
        int                order;
        const char        *accelerator;
    };
    std::span<const CurveEditorCommandInfo> CurveEditorCommands();

    struct CurveEditorCallbacks {
        // Model replacements must update the supplied history (Execute, Undo/Redo, or Clear).
        std::function<UndoCommandPtr(std::string, curve::Curve)> makeEdit;
        std::function<const curve::Curve &()>                    current;
    };

    // Reusable curve authoring widget: interactive graph, multi-key selection,
    // key list and inspector. The host owns undo/history and persistence.
    class CurveEditorWidget final : public sky::ui::UIElement {
    public:
        CurveEditorWidget(sky::ui::UITextSystem *text, CommandService &commands, CurveEditorCallbacks callbacks);
        ~CurveEditorWidget() override;

        void                    SetCurve(const curve::Curve &value);
        const curve::Curve     &GetCurve() const;
        void                    Tick(float delta);
        bool                    Editing() const;
        void                    Cancel();
        bool                    CanExecute(CurveEditorCommand command) const;
        bool                    AcceptsCommandShortcuts() const;
        bool                    SnapEnabled() const;
        void                    Execute(CurveEditorCommand command);
        const std::vector<int> &Selection() const;
        int                     PrimarySelection() const;

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        void                   ArrangeChildren(const sky::ui::UIRect &content) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        bool                   WantsPointerCapture() const override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;
        void                   OnPointerCancel(uint32_t pointer) override;
        void                   OnPointerButtonConflict(uint32_t pointer) override;
        void                   OnPointerLeave(const sky::ui::UIPointerEvent &event) override;

    private:
        class Graph;
        class KeyList;
        friend class Graph;
        friend class KeyList;
        void SetSelection(std::vector<int> indices, int primary);
        void SynchronizeCurrent();
        void BindSelection();
        void Commit(std::string label, curve::Curve next);
        void AddKeyAt(float time);
        void RemoveSelected();
        void CopySelected();
        void Paste(bool duplicate);
        void SetInterpolation(curve::CurveInterpolation value);

        curve::Curve                 curve;
        CurveEditorCallbacks         callbacks;
        CommandService              &commands;
        sky::ui::UITextSystem       *text  = nullptr;
        Graph                       *graph = nullptr;
        KeyList                     *list  = nullptr;
        ReflectedFormView           *form  = nullptr;
        std::vector<int>             selection;
        int                          primarySelection = -1;
        std::vector<curve::CurveKey> clipboard;
        float                        lastCursorTime       = 0.f;
        bool                         hasCursorTime        = false;
        bool                         snap                 = true;
        bool                         syncPending          = false;
        CommandService::CallbackId   beforeHistory        = 0;
        uint64_t                     modelRevision        = 0;
        uint64_t                     propertyEditRevision = 0;
    };

} // namespace sky::editor
