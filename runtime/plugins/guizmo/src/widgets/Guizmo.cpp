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
            renderScene = static_cast<RenderSceneProxy*>(world->GetSubSystem(Name("RenderScene")))->GetRenderScene();
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
        auto *sceneView = renderScene->GetSceneView(Name("MainCamera"));
        SKY_ASSERT(sceneView != nullptr);
        const auto &proj = sceneView->GetProject();
        auto view = sceneView->GetView();

//        ImGuizmo::DrawGrid(view.v, proj.v, identity.v, 500.f);
        auto offsetX = io.DisplaySize.x - 256;

        ImGuizmo::ViewManipulate(view.v, 8.f, ImVec2(offsetX, 0), ImVec2(256, 266), 0x10101010);

        if (actor != nullptr) {
            auto *transComp = actor->GetComponent<TransformComponent>();
            auto matrix = transComp->GetLocalTransform().ToMatrix();
            ImGuizmo::Manipulate(view.v, proj.v, ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE, ImGuizmo::LOCAL, matrix.v,
                                 nullptr, nullptr, nullptr, nullptr);
            Transform res = {};
            Decompose(matrix, res.translation, res.rotation, res.scale);
            transComp->SetLocalTransform(res);
        }
    }

} // namespace sky::editor