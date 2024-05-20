//
// Created by blues on 2024/5/21.
//

#include <editor/widgets/Guizmo.h>
#include <ImGuizmo.h>
#include <framework/world/TransformComponent.h>
#include <render/adaptor/components/CameraComponent.h>
#include <core/math/MathUtil.h>

namespace sky::editor {

    GuiZmoWidget::GuiZmoWidget() : ImWidget("GuiZmoWidget")
    {
        MainViewportEvent::Connect(this);
        SelectEvent::Connect(this);
    }

    GuiZmoWidget::~GuiZmoWidget()
    {
        MainViewportEvent::DisConnect(this);
        SelectEvent::DisConnect(this);
    }

    void GuiZmoWidget::Execute(ImContext &context)
    {
        static const auto &identity = Matrix4::Identity();
        ImGuizmo::BeginFrame();
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

        if (viewport == nullptr) {
            return;
        }
        auto *camera = viewport->GetComponent<CameraComponent>();
        SKY_ASSERT(camera != nullptr);
        const auto &proj = camera->GetProject();
        auto view = camera->GetView();

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