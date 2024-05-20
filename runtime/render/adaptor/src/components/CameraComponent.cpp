//
// Created by Zach Lee on 2021/12/1.
//

#include <render/adaptor/components/CameraComponent.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/world/TransformComponent.h>
#include <core/math/MathUtil.h>

#include <render/adaptor/Util.h>
#include <render/adaptor/interface/IMainViewport.h>
#include <render/Renderer.h>

namespace sky {
    CameraComponent::~CameraComponent()
    {
        ShutDown();
    }

    void CameraComponent::Reflect(SerializationContext *context)
    {
        context->Register<ProjectType>("ProjectType")
            .Enum(ProjectType::PROJECTIVE, "Projective")
            .Enum(ProjectType::ORTHOGONAL, "Orthogonal");

        context->Register<CameraComponent>("CameraComponent")
            .Member<&CameraComponent::near>("near")
            .Member<&CameraComponent::far>("far")
            .Member<&CameraComponent::fov>("fov")
            .Member<&CameraComponent::type>("type");
//            .Property(UI_PROP_VISIBLE, false);
    }

    void CameraComponent::Perspective(float near_, float far_, float fov_)
    {
        near   = near_;
        far    = far_;
        fov    = fov_;
        type   = ProjectType::PROJECTIVE;
    }

    void CameraComponent::Otho(float height)
    {
        othoH = height;
        type   = ProjectType::ORTHOGONAL;
    }

    void CameraComponent::SetAspect(uint32_t width, uint32_t height)
    {
        aspect = static_cast<float>(width) / static_cast<float>(height);
    }

    void CameraComponent::Tick(float time)
    {
        auto *transform = actor->GetComponent<TransformComponent>();
        auto world = transform->GetWorldMatrix();
        sceneView->SetMatrix(world);

        if (type == ProjectType::PROJECTIVE) {
            sceneView->SetProjective(near, far, fov / 180.f * 3.14f, aspect);
        }
    }

    const Matrix4 &CameraComponent::GetProject() const
    {
        return sceneView != nullptr ? sceneView->GetProject() : Matrix4::Identity();
    }

    const Matrix4 &CameraComponent::GetView() const
    {
        return sceneView != nullptr ? sceneView->GetView() : Matrix4::Identity();
    }

    void CameraComponent::OnActive()
    {
        sceneView = GetRenderSceneFromActor(actor)->CreateSceneView(1);
        MainViewportEvent::BroadCast(&IMainViewportEvent::Active, actor);
    }

    void CameraComponent::OnDeActive()
    {
        MainViewportEvent::BroadCast(&IMainViewportEvent::DeActive);
        ShutDown();
    }

    void CameraComponent::ShutDown()
    {
        if (sceneView != nullptr) {
            GetRenderSceneFromActor(actor)->RemoveSceneView(sceneView);
            sceneView = nullptr;
        }
    }

    void CameraComponent::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("near"), near);
        ar.SaveValueObject(std::string("far"), far);
        ar.SaveValueObject(std::string("fov"), fov);
        ar.SaveValueObject(std::string("aspect"), aspect);
        ar.SaveValueObject(std::string("othoH"), othoH);
        ar.SaveValueObject(std::string("type"), type);
        ar.EndObject();
    }

    void CameraComponent::LoadJson(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("near", near);
        ar.LoadKeyValue("far", far);
        ar.LoadKeyValue("fov", fov);
        ar.LoadKeyValue("aspect", aspect);
        ar.LoadKeyValue("othoH", othoH);
        ar.LoadKeyValue("type", type);
    }
} // namespace sky
