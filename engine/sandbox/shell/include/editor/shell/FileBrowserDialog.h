//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/filebrowser/FileBrowserModel.h>
#include <editor/core/text/TextEditState.h>
#include <editor/shell/ModalDialog.h>
#include <editor/shell/UiSkin.h>

#include <ui/UIElement.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <functional>
#include <string>
#include <vector>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Engine-drawn modal file/directory chooser, laid out like a Blender/Unreal
    // browser: a Places sidebar, a location toolbar, a Name/Type list, and a
    // filter + Name + OK/Cancel footer. Backed by a headless FileBrowserModel, so
    // the same control serves filesystem picks and (later) asset-type picks.
    class FileBrowserDialog : public ModalDialog {
    public:
        explicit FileBrowserDialog(sky::ui::UITextSystem *text);
        ~FileBrowserDialog() override = default;

        const char *GetTypeName() const override
        {
            return "FileBrowserDialog";
        }

        void SetTextSystem(sky::ui::UITextSystem *text)
        {
            textSystem = text;
        }

        void Open(const FileBrowserRequest &request);
        void Close();

        FileBrowserModel &Model()
        {
            return model;
        }
        const FileBrowserModel &Model() const
        {
            return model;
        }

        void SetOnResult(std::function<void(const FileBrowserResult &)> callback)
        {
            onResult = std::move(callback);
        }

        // Write mode: the browser is choosing a location to create in (Select
        // Directory, e.g. New Project), so creation affordances are offered.
        bool WriteMode() const
        {
            return model.GetMode() == FileBrowserMode::SELECT_DIRECTORY;
        }

        void                   OnPaint(sky::ui::UIPaintContext &context) override;
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override;
        sky::ui::UIEventResult OnKeyEvent(const sky::ui::UIKeyEvent &event) override;
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &event) override;

        // Layout queries (public so hosts and tests can target regions without
        // duplicating constants).
        sky::ui::UIRect PanelRect() const;
        sky::ui::UIRect SidebarRect() const;
        sky::ui::UIRect PlaceRowRect(int index) const;
        sky::ui::UIRect ToolbarRect() const;
        sky::ui::UIRect UpRect() const;
        sky::ui::UIRect PathRect() const;
        sky::ui::UIRect NewFolderRect() const;
        sky::ui::UIRect ListHeaderRect() const;
        sky::ui::UIRect ListRect() const;
        sky::ui::UIRect RowRect(int index) const;
        sky::ui::UIRect FooterRect() const;
        sky::ui::UIRect FilterRect() const;
        sky::ui::UIRect FilterItemRect(int index) const; // 0 = "All", i+1 = filters[i]
        sky::ui::UIRect NameFieldRect() const;
        sky::ui::UIRect ButtonRect(int index) const; // 0 = OK, 1 = Cancel

        sky::ui::UIRect ContextMenuRect() const;
        sky::ui::UIRect ContextItemRect(int index) const;

        int PlaceAt(float x, float y) const;
        int RowAt(float x, float y) const;
        int FilterItemAt(float x, float y) const;
        int ButtonAt(float x, float y) const;
        int ContextItemAt(float x, float y) const;

    private:
        int  FilterItemCount() const;
        void Accept();
        void Cancel();

        int   NameCaretFromX(float x) const;
        float NameCaretX() const;
        void  SyncName(); // model.SetName(nameEdit.GetText())

        int                      ContextItemCount() const;
        std::vector<std::string> ContextItems() const;
        void                     BeginRename(const std::string &name);
        void                     CommitRename();

        // Painting (split for readability; the element is fully self-drawn).
        void PaintChrome(sky::ui::UIPaintContext &context) const;
        void PaintSidebar(sky::ui::UIPaintContext &context) const;
        void PaintToolbar(sky::ui::UIPaintContext &context) const;
        void PaintList(sky::ui::UIPaintContext &context) const;
        void PaintFooter(sky::ui::UIPaintContext &context) const;
        void PaintFilterPopup(sky::ui::UIPaintContext &context) const;
        void PaintContextMenu(sky::ui::UIPaintContext &context) const;

        sky::ui::UIEventResult HandlePointerDown(float x, float y);

        sky::ui::UITextSystem                         *textSystem = nullptr;
        UiSkin                                         skin;
        FileBrowserModel                               model;
        std::function<void(const FileBrowserResult &)> onResult;
        TextEditState                                  nameEdit;

        bool      filterPopupOpen  = false;
        int       hoverPlace       = -1;
        int       hoverRow         = -1;
        int       hoverFilterItem  = -1;
        int       hoverButton      = -1;
        bool      hoverUp          = false;
        bool      hoverNewFolder   = false;
        bool      hoverFilter      = false;
        int       lastClickRow     = -1;
        long long lastClickMs      = 0;
        bool      renameActive     = false;
        bool      contextMenuOpen  = false;
        int       contextMenuRow   = -1;
        float     contextX         = 0.0f;
        float     contextY         = 0.0f;
        int       hoverContextItem = -1;
    };

} // namespace sky::editor
