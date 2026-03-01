//
// Created by blues on 2024/3/31.
//

#include <editor/widgets/WorldWidget.h>
#include <framework/interface/ISelectEvent.h>
#include <imgui.h>

namespace sky::editor {

    void WorldWidget::Execute(ImContext &context)
    {
        if (!show) {
            return;
        }

        if (ImGui::Begin("WorldInfo")) {
            ShowActors();
            ImGui::End();
        }
    }

    void WorldWidget::ShowActors()
    {
        if (world == nullptr) {
            return;
        }
        const auto &actors = world->GetActors();

        if (ImGui::TreeNode("World")) {

            for (const auto &actor: actors) {
                if (ImGui::TreeNodeEx(actor->GetName().c_str(), ImGuiTreeNodeFlags_Leaf)) {
                    if (ImGui::IsItemClicked()) {
                        SelectEvent::BroadCast(&ISelectEvent::OnActorSelected, actor.get());
                    }
                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }
    }

    void WorldWidget::BindEvent(EventID id)
    {
        binder.Bind(this, id);
    }
} // namespace sky::editor