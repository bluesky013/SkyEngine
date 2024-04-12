//
// Created by blues on 2024/3/29.
//

#include <editor/widgets/DemoWidget.h>

namespace sky::editor {

    void DemoWidget::Execute(ImContext &context)
    {
        if (show) {
            ImGui::ShowDemoWindow(nullptr);
            ImPlot::ShowDemoWindow(nullptr);
        }
    }

    void DemoWidget::BindEvent(EventID id)
    {
        binder.Bind(this, id);
    }

} // sky::editor