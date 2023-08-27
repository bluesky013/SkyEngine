//
// Created by Zach Lee on 2021/12/1.
//

#include <render/adaptor/components/CameraComponent.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/world/GameObject.h>
#include <framework/world/World.h>

#include <render/adaptor/Util.h>
#include <render/Renderer.h>

namespace sky {

    void CameraComponent::Reflect()
    {
        SerializationContext::Get()
            ->Register<ProjectType>("ProjectType")()
            .Property(ProjectType::PROJECTIVE, std::string("PROJECTIVE"))
            .Property(ProjectType::ORTHOGONAL, std::string("ORTHOGONAL"));

        SerializationContext::Get()
            ->Register<CameraComponent>(NAME)
            .Member<&CameraComponent::near>("near")
            .Member<&CameraComponent::far>("far")
            .Member<&CameraComponent::fov>("fov")
            .Member<&CameraComponent::aspect>("aspect")
            .Member<&CameraComponent::type>("projectType")
            .Property(UI_PROP_VISIBLE, false);

        ComponentFactory::Get()->RegisterComponent<CameraComponent>();
    }

    void CameraComponent::Perspective(float near_, float far_, float fov_, float aspect_)
    {
        near   = near_;
        far    = far_;
        fov    = fov_;
        aspect = aspect_;
        type   = ProjectType::PROJECTIVE;
    }

    void CameraComponent::Otho(float height)
    {
        othoH = height;
        type   = ProjectType::ORTHOGONAL;
    }

    void CameraComponent::OnTick(float time)
    {
        if (dirty) {
            if (type == ProjectType::PROJECTIVE) {
                sceneView->SetProjective(near, far, fov, aspect);
            }
            dirty = false;
        }
    }

    void CameraComponent::OnActive()
    {
        sceneView = GetRenderSceneFromGameObject(object)->CreateSceneView(1);
    }

    void CameraComponent::OnDestroy()
    {
        GetRenderSceneFromGameObject(object)->RemoveSceneView(sceneView);
    }

    void CameraComponent::Save(JsonOutputArchive &ar) const
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

    void CameraComponent::Load(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("near", near);
        ar.LoadKeyValue("far", far);
        ar.LoadKeyValue("fov", fov);
        ar.LoadKeyValue("aspect", aspect);
        ar.LoadKeyValue("othoH", othoH);
        ar.LoadKeyValue("type", type);
    }
} // namespace sky