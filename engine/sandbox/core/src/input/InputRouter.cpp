//
// Created on 2026/09/21.
//

#include <editor/core/input/InputRouter.h>
#include <utility>

namespace sky::editor {

    void InputRouter::SetFocus(std::string panelId)
    {
        focusPanel = std::move(panelId);
        focusConsumesInput = false;
    }

    void InputRouter::ClearFocus()
    {
        focusPanel.clear();
        focusConsumesInput = false;
    }

    void InputRouter::PushModal()
    {
        ++modalDepth;
    }

    void InputRouter::PopModal()
    {
        if (modalDepth > 0) {
            --modalDepth;
        }
    }

} // namespace sky::editor
