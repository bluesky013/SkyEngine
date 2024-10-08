//
// Created by blues on 2024/3/31.
//

#pragma once

#include <imgui/ImWidget.h>
#include <editor/event/Event.h>
#include <framework/world/World.h>

namespace sky::editor {
    class WorldWidget : public ImWidget, public IToggleEvent {
    public:
        WorldWidget() : ImWidget("World") {}
        ~WorldWidget() override = default;

        void SetWorld(const WorldPtr &world_) { world = world_; }
        void BindEvent(EventID id);

    private:
        void Execute(ImContext &context) override;
        void ShowActors();
        void OnToggle(bool val) override { show = val; }

        bool show = false;
        WorldPtr world = nullptr;

        EventBinder<IToggleEvent> binder;
    };

} // namespace sky::editor