//
// Created on 2026/10/06.
//

#include <editor/widgets/StatusBar.h>

#include <editor/ui/UiDraw.h>
#include <editor/ui/UiTheme.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

namespace sky::editor {

    namespace uc = uidraw;

    StatusBar::StatusBar(sky::ui::UITextSystem *text) : textSystem(text)
    {
    }

    void StatusBar::OnPaint(sky::ui::UIPaintContext &context)
    {
        const UiTheme        &th = GetDefaultUiTheme();
        const sky::ui::UIRect b  = GetBounds();
        uc::Fill(context, b, th.colors.toolbar);
        uc::HLine(context, b.left, b.right, b.top, th.colors.borderSoft);
        uc::Text(context, text, th.fonts.label, sky::ui::UIRect{b.left + th.metrics.controlPad, b.top, b.right - th.metrics.controlPad, b.bottom},
                 th.colors.text, textSystem);
    }

} // namespace sky::editor
