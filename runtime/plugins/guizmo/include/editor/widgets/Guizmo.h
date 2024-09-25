//
// Created by blues on 2024/5/21.
//

#pragma once

#include <imgui/ImWidget.h>
#include <editor/widgets/WorldWidget.h>
#include <core/math/Matrix4.h>
#include <framework/interface/ISelectEvent.h>

namespace sky {
    class RenderScene;
} // namespace sky

namespace sky::editor {
    class GuiZmoWidget : public ImWidget, public ISelectEvent {
    public:
        GuiZmoWidget();
        ~GuiZmoWidget() override;

        void Execute(ImContext &context) override;
        void AttachScene(RenderScene *scn) { renderScene = scn; }
    private:
        void OnActorSelected(Actor *actor_) override;

        Actor *actor = nullptr;
        RenderScene* renderScene = nullptr;
    };

} // namespace sky::editor