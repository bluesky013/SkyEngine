//
// Created by blues on 2024/3/31.
//

#include <editor/widgets/WorldWidget.h>
#include <imgui.h>

namespace sky::editor {

    void WorldWidget::Execute(ImContext &context)
    {
        if (!show) {
            return;
        }
    }

    void WorldWidget::BindEvent(EventID id)
    {
        binder.Bind(this, id);
    }
} // namespace sky::editor