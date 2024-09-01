//
// Created by blues on 2024/5/21.
//

#include <editor/widgets/Guizmo.h>
#include <ImGuizmo.h>
#include <framework/world/TransformComponent.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <core/math/MathUtil.h>

namespace sky::editor {

    GuiZmoWidget::GuiZmoWidget() : ImWidget("GuiZmoWidget")
    {
        SelectEvent::Connect(this);
    }

    GuiZmoWidget::~GuiZmoWidget()
    {
        SelectEvent::DisConnect(this);
    }

    void GuiZmoWidget::OnActorSelected(Actor *actor_)
    {
        actor = actor_;

        if (actor != nullptr) {
            auto *world = actor->GetWorld();
            renderScene = static_cast<RenderSceneProxy*>(world->GetSubSystem("RenderScene"))->GetRenderScene();
        }
    }

    void GuiZmoWidget::Execute(ImContext &context)
    {
        static const auto &identity = Matrix4::Identity();
        ImGuizmo::BeginFrame();
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

        if (renderScene == nullptr) {
            return;
        }
        auto *sceneView = renderScene->GetSceneViews()[0].get();
        SKY_ASSERT(sceneView != nullptr);
        const auto &proj = sceneView->GetProject();
        auto view = sceneView->GetView();

        // ImGuizmo::DrawGrid(viewMatrix.v, projectMatrix.v, identity.v, 500.f);
        ImGuizmo::ViewManipulate(view.v, 8.f, ImVec2(0, 0), ImVec2(128, 128), 0x10101010);

        if (actor != nullptr) {
            auto *transComp = actor->GetComponent<TransformComponent>();
            auto matrix = transComp->GetLocalTransform().ToMatrix();
            ImGuizmo::Manipulate(view.v, proj.v, ImGuizmo::UNIVERSAL, ImGuizmo::LOCAL, matrix.v,
                                 nullptr, nullptr, nullptr, nullptr);
            Transform res = {};
            Decompose(matrix, res.translation, res.rotation, res.scale);
            transComp->SetLocalTransform(res);
        }
    }

} // namespace sky::editor