//
// Created by blues on 2024/5/21.
//

#pragma once

#include <imgui/ImWidget.h>
#include <editor/widgets/WorldWidget.h>
#include <core/math/Matrix4.h>
#include <render/adaptor/interface/IMainViewport.h>

namespace sky::editor {

    class GuiZmoWidget : public ImWidget, public IMainViewportEvent, public ISelectEvent {
    public:
        GuiZmoWidget();
        ~GuiZmoWidget() override;

        void Execute(ImContext &context) override;
    private:
        void Active(Actor *actor_) override { viewport = actor_; }
        void DeActive() override { viewport = nullptr; }

        void OnActorSelected(Actor *actor_) override { actor = actor_; }

        Actor *viewport = nullptr;
        Actor *actor = nullptr;
    };

} // namespace sky::editor