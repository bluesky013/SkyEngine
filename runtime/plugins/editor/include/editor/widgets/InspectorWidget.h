//
// Created by blues on 2024/5/19.
//

#pragma once

#include <imgui/ImWidget.h>
#include <editor/event/Event.h>
#include <editor/widgets/WorldWidget.h>
#include <framework/world/World.h>

namespace sky::editor {

    class InspectorWidget : public ImWidget, public IToggleEvent, public ISelectEvent  {
    public:
        InspectorWidget();
        ~InspectorWidget() override;

        void BindEvent(EventID id);
    private:
        void Execute(ImContext &context) override;
        void OnToggle(bool val) override { show = val; }
        void OnActorSelected(Actor *actor_) override { actor = actor_; }
        void ShowDetails();
        static void ShowComponent(const Uuid &id, ComponentBase *comp);

        bool show = false;
        Actor *actor = nullptr;
        EventBinder<IToggleEvent> binder;
    };

} // namespace sky::editor