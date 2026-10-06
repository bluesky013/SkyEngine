//
// Created on 2026/10/06.
//

#include <editor/shell/widgets/StatusBar.h>

#include <editor/shell/UiDraw.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

namespace sky::editor {

    namespace uc = uidraw;

    StatusBar::StatusBar(sky::ui::UITextSystem *text)
        : textSystem(text)
    {
    }

    void StatusBar::OnPaint(sky::ui::UIPaintContext &context)
    {
        const UiTheme &th = GetDefaultUiTheme();
        const sky::ui::UIRect b = GetBounds();
        uc::Fill(context, b, th.colors.toolbar);
        uc::HLine(context, b.left, b.right, b.top, th.colors.borderSoft);
        uc::Text(context, text, 12, sky::ui::UIRect{b.left + 8.0f, b.top, b.right - 8.0f, b.bottom}, th.colors.text,
                 textSystem);
    }

} // namespace sky::editor
