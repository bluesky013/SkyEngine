//
// Created on 2026/09/19.
//

#include <ui/UIPaintContext.h>
#include <ui/UIStyle.h>
#include <ui/widgets/Panel.h>

namespace sky::ui {

    Panel::Panel()
    {
        SetLayoutVolatile(false);
        SetClipsChildren(true);
    }

    void Panel::OnPaint(UIPaintContext &context)
    {
        const UITheme *theme = context.GetTheme();
        if (theme == nullptr) {
            return;
        }

        const UIStyle style = theme->Resolve(GetStyleClasses());
        if (style.backgroundColor == 0) {
            return;
        }
        context.AddRect(GetBounds(), style.backgroundColor);
    }

} // namespace sky::ui
