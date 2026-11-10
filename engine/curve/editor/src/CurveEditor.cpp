#include <curve/editor/CurveEditor.h>

#include <core/logger/Logger.h>
#include <curve/editor/CurveDocument.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/CurveEditorWidget.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

namespace sky::curve::editor {
    namespace {
        constexpr const char *TAG = "CurveEditor";
    } // namespace

    CurveEditorView::CurveEditorView(CurveDocument &inDocument, sky::ui::UITextSystem *inText, std::string scope)
        : document(inDocument), actionScope(std::move(scope))
    {
        sky::editor::CurveEditorCallbacks callbacks;
        callbacks.makeEdit = [this](std::string label, Curve next) { return document.MakeEdit(std::move(label), std::move(next)); };
        callbacks.current  = [this]() -> const Curve  &{ return document.GetCurve(); };
        auto widget        = std::make_unique<sky::editor::CurveEditorWidget>(inText, document.Commands(), std::move(callbacks));
        editor             = static_cast<sky::editor::CurveEditorWidget *>(AddChild(std::move(widget)));
        editor->SetCurve(document.GetCurve());
        if (!actionScope.empty()) {
            for (const auto &command : sky::editor::CurveEditorCommands()) {
                sky::editor::EditorAction action;
                action.id                  = command.id;
                action.label               = command.label;
                action.icon                = command.icon;
                action.group               = sky::editor::EditorGroup::History;
                action.menu                = sky::editor::EditorMenu::Edit;
                action.submenu             = "Curve";
                action.scope               = actionScope;
                action.order               = command.order;
                action.accelerator         = command.accelerator;
                action.acceleratorEligible = [this] { return editor && editor->AcceptsCommandShortcuts(); };
                if (command.command == sky::editor::CurveEditorCommand::ToggleSnap) {
                    action.checkable = true;
                    action.checked   = [this] { return editor && editor->SnapEnabled(); };
                }
                action.enabled = [this, value = command.command] { return editor != nullptr && editor->CanExecute(value); };
                action.invoke  = [this, value = command.command] {
                    if (editor)
                        editor->Execute(value);
                    error = document.LastError();
                };
                sky::editor::EditorActionRegistry::Get()->Add(std::move(action));
            }
        }
    }
    CurveEditorView::~CurveEditorView()
    {
        if (!actionScope.empty()) {
            for (const auto &command : sky::editor::CurveEditorCommands())
                sky::editor::EditorActionRegistry::Get()->Remove(actionScope, command.id);
        }
        ClearChildren();
        editor = nullptr;
    }
    void CurveEditorView::Tick(float delta)
    {
        if (editor)
            editor->Tick(delta);
        error = document.LastError();
    }
    void CurveEditorView::OnPaint(sky::ui::UIPaintContext &context)
    {
        context.AddRect(GetBounds(), sky::editor::GetDefaultUiTheme().colors.panel);
    }
    sky::ui::UISize CurveEditorView::OnMeasure(const sky::ui::UIConstraints &constraints)
    {
        sky::ui::UISize size = constraints.maxSize;
        if (!std::isfinite(size.width))
            size.width = sky::editor::GetDefaultUiTheme().metrics.fieldWidth * 2;
        if (!std::isfinite(size.height))
            size.height = sky::editor::GetDefaultUiTheme().metrics.dialogMinHeight;
        return size;
    }
    void CurveEditorView::ArrangeChildren(const sky::ui::UIRect &content)
    {
        if (!editor)
            return;
        const auto &theme = sky::editor::GetDefaultUiTheme();
        const float pad   = theme.metrics.controlPad;
        editor->Arrange({content.left + pad, content.top + pad, content.right - pad, content.bottom - pad});
    }
    sky::ui::UIEventResult CurveEditorView::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        return editor ? editor->OnPointerEvent(event) : sky::ui::UIEventResult::UNHANDLED;
    }
    bool CurveEditorView::WantsPointerCapture() const
    {
        return editor && editor->WantsPointerCapture();
    }
    sky::ui::UIEventResult CurveEditorView::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        return editor ? editor->OnKeyEvent(event) : sky::ui::UIEventResult::UNHANDLED;
    }
    sky::ui::UIEventResult CurveEditorView::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        return editor ? editor->OnTextInput(event) : sky::ui::UIEventResult::UNHANDLED;
    }
    void CurveEditorView::OnPointerCancel(uint32_t pointer)
    {
        if (editor)
            editor->OnPointerCancel(pointer);
    }
    void CurveEditorView::OnPointerButtonConflict(uint32_t pointer)
    {
        OnPointerCancel(pointer);
    }
    void CurveEditorView::OnPointerLeave(const sky::ui::UIPointerEvent &event)
    {
        if (editor)
            editor->OnPointerLeave(event);
    }
    bool CurveEditorView::Editing() const
    {
        return editor && editor->Editing();
    }
    void CurveEditorView::Cancel()
    {
        if (editor)
            editor->Cancel();
    }
    sky::editor::CommandService &CurveEditorView::History()
    {
        return document.Commands();
    }
    const std::string &CurveEditorView::Error() const
    {
        return error.empty() ? document.LastError() : error;
    }

    namespace {
        class CurveEditorDocument final : public sky::editor::IAssetViewDocument {
        public:
            explicit CurveEditorDocument(std::unique_ptr<CurveDocument> document) : document(std::move(document))
            {
            }
            void SetActionScope(std::string scope) override
            {
                actionScope = std::move(scope);
            }
            bool FitsContentToViewport() const override
            {
                return true;
            }
            std::unique_ptr<sky::ui::UIElement> CreateContent(sky::ui::UITextSystem *text) override
            {
                auto content = std::make_unique<CurveEditorView>(*document, text, actionScope);
                view         = content.get();
                return content;
            }
            sky::editor::CommandService &History() override
            {
                return document->Commands();
            }
            bool IsDirty() const override
            {
                return document->IsDirty();
            }
            bool IsEditing() const override
            {
                return view && view->Editing();
            }
            void CancelInteractions() override
            {
                if (view)
                    view->Cancel();
            }
            void RequestSave() override
            {
                document->RequestSave();
            }
            bool IsSavePending() const override
            {
                return document->IsSavePending();
            }
            std::string Status() const override
            {
                std::string status = document->IsDirty() ? "Unsaved draft. " : "Saved. ";
                if (!document->LastError().empty())
                    return status + document->LastError();
                return status + "Curve source is valid";
            }
            void Tick(float delta) override
            {
                if (!IsEditing())
                    document->FlushPendingSave();
                if (view)
                    view->Tick(delta);
            }

        private:
            std::unique_ptr<CurveDocument> document;
            std::string                    actionScope;
            CurveEditorView               *view = nullptr;
        };

        class CurveViewProvider final : public sky::editor::IAssetViewProvider {
        public:
            bool UsesDocumentWindow() const override
            {
                return true;
            }
            sky::editor::AssetViewDocument OpenDocument(const Uuid &asset, const std::string &type) override
            {
                if (type != "Curve")
                    return {};
                auto document = std::make_unique<CurveDocument>(asset);
                if (!document->Load()) {
                    LOG_W(TAG, "curve document %s load failed: %s", asset.ToString().c_str(), document->LastError().c_str());
                    return {};
                }
                LOG_I(TAG, "curve document %s opened", asset.ToString().c_str());
                return sky::editor::AssetViewDocument{std::make_unique<CurveEditorDocument>(std::move(document))};
            }
        };
    } // namespace

    std::unique_ptr<sky::editor::IAssetViewDocument> CreateCurveEditorDocument(const Uuid &asset, const std::string &type)
    {
        CurveViewProvider provider;
        auto              opened = provider.OpenDocument(asset, type);
        return std::move(opened.document);
    }
    std::unique_ptr<sky::editor::IAssetViewProvider> CreateCurveViewProvider()
    {
        return std::make_unique<CurveViewProvider>();
    }
} // namespace sky::curve::editor
