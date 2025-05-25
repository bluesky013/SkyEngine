//
// Created by Zach Lee on 2025/5/25.
//

#include <editor/framework/ViewportWidget.h>

namespace sky::editor {

    void* ViewportWindow::GetNativeWindow()
    {
        return winId();
    }

} // namespace sky::editor
