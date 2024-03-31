//
// Created by blues on 2024/3/29.
//

#pragma once

#include <imgui/ImWidget.h>
#include <editor/event/Event.h>

namespace sky::editor {

    class DemoWidget : public ImWidget, public IToggleEvent {
    public:
        DemoWidget() : ImWidget("DemoWidget") {}
        ~DemoWidget() override = default;

        void Execute(ImContext &context) override;
        void BindEvent(EventID id);

        void OnToggle(bool val) override { show = val; }

    private:
        bool show = false;

        EventBinder<IToggleEvent> binder;
    };

} // namespace sky::editor