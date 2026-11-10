#pragma once

#include <core/util/Uuid.h>
#include <curve/Curve.h>
#include <editor/core/command/CommandService.h>
#include <editor/ui/AssetViewProvider.h>
#include <ui/UIElement.h>

#include <memory>
#include <string>

namespace sky::ui {
    class UITextSystem;
}
namespace sky::editor {
    class CurveEditorWidget;
}
namespace sky::curve::editor {
    class CurveDocument;

    class CurveEditorView final : public sky::ui::UIElement {
    public:
        CurveEditorView(CurveDocument &document, sky::ui::UITextSystem *text, std::string actionScope = {});
        ~CurveEditorView() override;
        void                         OnPaint(sky::ui::UIPaintContext &context) override;
        void                         ArrangeChildren(const sky::ui::UIRect &content) override;
        sky::ui::UIEventResult       OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        bool                         WantsPointerCapture() const override;
        sky::ui::UIEventResult       OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult       OnTextInput(const sky::ui::UITextInputEvent &event) override;
        void                         OnPointerCancel(uint32_t pointer) override;
        void                         OnPointerButtonConflict(uint32_t pointer) override;
        void                         OnPointerLeave(const sky::ui::UIPointerEvent &event) override;
        void                         Tick(float delta);
        bool                         Editing() const;
        void                         Cancel();
        sky::editor::CommandService &History();
        const std::string           &Error() const;

    protected:
        sky::ui::UISize OnMeasure(const sky::ui::UIConstraints &constraints) override;

    private:
        CurveDocument                  &document;
        sky::editor::CurveEditorWidget *editor = nullptr;
        std::string                     error;
        std::string                     actionScope;
    };

    std::unique_ptr<sky::editor::IAssetViewDocument> CreateCurveEditorDocument(const Uuid &asset, const std::string &type);
    std::unique_ptr<sky::editor::IAssetViewProvider> CreateCurveViewProvider();

} // namespace sky::curve::editor
