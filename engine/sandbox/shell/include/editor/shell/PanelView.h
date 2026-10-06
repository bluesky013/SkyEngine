//
// Created on 2026/10/06.
//

#pragma once

namespace sky::editor {

    // Optional capability of a panel body: show/hide its own title bar. The shell
    // hides it when it provides a tab header (so titles are not drawn twice).
    class IPanelChrome {
    public:
        virtual ~IPanelChrome() = default;
        virtual void SetTitleBarVisible(bool visible) = 0;
    };

} // namespace sky::editor
