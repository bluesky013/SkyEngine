//
// Created by blues on 2024/3/31.
//

#include <editor/widgets/WorldWidget.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <imgui.h>

namespace sky::editor {

    void WorldWidget::Traverse(Actor *actor)
    {
        auto *transform = actor->GetComponent<TransformComponent>();
        const auto &children = transform->GetChildren();
        if (!children.empty()) {
            if (ImGui::TreeNode(actor->GetName().c_str())) {
                for (auto *child : children) {
                    Traverse(child->object);
                }
                ImGui::TreePop();
            }
        } else {
            ImGui::Selectable(actor->GetName().c_str());
        }
    }

    void WorldWidget::Execute(ImContext &context)
    {
        if (!show) {
            return;
        }

        if (ImGui::Begin("World")) {
            auto *root = world->GetRoot();
            Traverse(root);
        }
        ImGui::End();
    }
} // namespace sky::editor