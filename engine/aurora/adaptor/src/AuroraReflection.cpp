//
// Aurora -> framework reflection / asset / component bridge implementation.
//

#include <aurora/adaptor/AuroraReflection.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/components/CameraComponent.h>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/PointLightComponent.h>
#include <aurora/adaptor/components/SpotLightComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/scene/SceneTypes.h>

#include <core/shapes/AABB.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>

#include <string>

namespace sky {

    void aurora::StaticMeshComponent::Reflect(SerializationContext *context)
    {
        context->Register<aurora::StaticMeshComponentData>("StaticMeshComponentData")
            .Member<&aurora::StaticMeshComponentData::mesh>("mesh")
            .Member<&aurora::StaticMeshComponentData::material>("material")
            .Member<&aurora::StaticMeshComponentData::castShadow>("castShadow")
            .Member<&aurora::StaticMeshComponentData::receiveShadow>("receiveShadow");

        context->Register<aurora::StaticMeshComponent>("StaticMeshComponent")
            .Member<&aurora::StaticMeshComponent::SetMeshUuid, &aurora::StaticMeshComponent::GetMeshUuid>("mesh")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<aurora::Mesh>::ASSET_TYPE))
            .Member<&aurora::StaticMeshComponent::SetMaterialUuid, &aurora::StaticMeshComponent::GetMaterialUuid>("material")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<aurora::Material>::ASSET_TYPE))
            .Member<&aurora::StaticMeshComponent::SetCastShadow, &aurora::StaticMeshComponent::GetCastShadow>("castShadow")
            .Member<&aurora::StaticMeshComponent::SetReceiveShadow, &aurora::StaticMeshComponent::GetReceiveShadow>("receiveShadow");
    }

    void aurora::StaticMeshComponent::OnAssetLoaded(const Uuid & /*uuid*/, const std::string_view & /*type*/)
    {
        // Device-side resource build (mesh renderer / material binding) is a
        // follow-up; the component tracks the asset handles here.
    }

    void aurora::CameraComponent::Reflect(SerializationContext *context)
    {
        context->Register<aurora::CameraComponentData>("CameraComponentData")
            .Member<&aurora::CameraComponentData::fov>("fov")
            .Member<&aurora::CameraComponentData::nearZ>("nearZ")
            .Member<&aurora::CameraComponentData::farZ>("farZ");

        context->Register<aurora::CameraComponent>("CameraComponent")
            .Member<&aurora::CameraComponent::SetFov, &aurora::CameraComponent::GetFov>("fov")
            .Member<&aurora::CameraComponent::SetNearZ, &aurora::CameraComponent::GetNearZ>("nearZ")
            .Member<&aurora::CameraComponent::SetFarZ, &aurora::CameraComponent::GetFarZ>("farZ");
    }

    void aurora::DirectLightComponent::Reflect(SerializationContext *context)
    {
        context->Register<aurora::DirectLightData>("DirectLightData")
            .Member<&aurora::DirectLightData::color>("color")
            .Member<&aurora::DirectLightData::intensity>("intensity")
            .Member<&aurora::DirectLightData::castShadow>("castShadow");

        context->Register<aurora::DirectLightComponent>("DirectLightComponent")
            .Member<&aurora::DirectLightComponent::SetColor, &aurora::DirectLightComponent::GetColor>("color")
            .Member<&aurora::DirectLightComponent::SetIntensity, &aurora::DirectLightComponent::GetIntensity>("intensity")
            .Member<&aurora::DirectLightComponent::SetCastShadow, &aurora::DirectLightComponent::GetCastShadow>("castShadow");
    }

    void aurora::PointLightComponent::Reflect(SerializationContext *context)
    {
        context->Register<aurora::PointLightData>("PointLightData")
            .Member<&aurora::PointLightData::color>("color")
            .Member<&aurora::PointLightData::intensity>("intensity")
            .Member<&aurora::PointLightData::range>("range");

        context->Register<aurora::PointLightComponent>("PointLightComponent")
            .Member<&aurora::PointLightComponent::SetColor, &aurora::PointLightComponent::GetColor>("color")
            .Member<&aurora::PointLightComponent::SetIntensity, &aurora::PointLightComponent::GetIntensity>("intensity")
            .Member<&aurora::PointLightComponent::SetRange, &aurora::PointLightComponent::GetRange>("range");
    }

    void aurora::SpotLightComponent::Reflect(SerializationContext *context)
    {
        context->Register<aurora::SpotLightData>("SpotLightData")
            .Member<&aurora::SpotLightData::color>("color")
            .Member<&aurora::SpotLightData::intensity>("intensity")
            .Member<&aurora::SpotLightData::range>("range")
            .Member<&aurora::SpotLightData::innerConeAngle>("innerConeAngle")
            .Member<&aurora::SpotLightData::outerConeAngle>("outerConeAngle");

        context->Register<aurora::SpotLightComponent>("SpotLightComponent")
            .Member<&aurora::SpotLightComponent::SetColor, &aurora::SpotLightComponent::GetColor>("color")
            .Member<&aurora::SpotLightComponent::SetIntensity, &aurora::SpotLightComponent::GetIntensity>("intensity")
            .Member<&aurora::SpotLightComponent::SetRange, &aurora::SpotLightComponent::GetRange>("range")
            .Member<&aurora::SpotLightComponent::SetInnerConeAngle, &aurora::SpotLightComponent::GetInnerConeAngle>("innerConeAngle")
            .Member<&aurora::SpotLightComponent::SetOuterConeAngle, &aurora::SpotLightComponent::GetOuterConeAngle>("outerConeAngle");
    }

    namespace aurora {

        static void ReflectSceneTypes(SerializationContext *context)
        {
            context->Register<AABB>("AABB")
                .Member<&AABB::min>("min")
                .Member<&AABB::max>("max");

            context->Register<sky::aurora::Bounds>("Bounds")
                .Member<&sky::aurora::Bounds::worldBounds>("worldBounds");

            context->Register<sky::aurora::WorldInfo>("WorldInfo")
                .Member<&sky::aurora::WorldInfo::world>("world");

            context->Register<sky::aurora::LightType>("LightType")
                .Enum(sky::aurora::LightType::DIRECTIONAL, "DIRECTIONAL")
                .Enum(sky::aurora::LightType::POINT, "POINT")
                .Enum(sky::aurora::LightType::SPOT, "SPOT");

            context->Register<sky::aurora::Light>("Light")
                .Member<&sky::aurora::Light::type>("type")
                .Member<&sky::aurora::Light::color>("color")
                .Member<&sky::aurora::Light::intensity>("intensity")
                .Member<&sky::aurora::Light::direction>("direction")
                .Member<&sky::aurora::Light::position>("position")
                .Member<&sky::aurora::Light::range>("range")
                .Member<&sky::aurora::Light::innerConeAngle>("innerConeAngle")
                .Member<&sky::aurora::Light::outerConeAngle>("outerConeAngle");
        }

        static void ReflectAssetTypes(SerializationContext *context)
        {
            context->Register<sky::aurora::MeshAssetData>("MeshAssetData")
                .BinLoad<&sky::aurora::MeshAssetData::Load>()
                .BinSave<&sky::aurora::MeshAssetData::Save>();

            context->Register<sky::aurora::MaterialAssetData>("MaterialAssetData")
                .BinLoad<&sky::aurora::MaterialAssetData::Load>()
                .BinSave<&sky::aurora::MaterialAssetData::Save>();

            context->Register<sky::aurora::ImageAssetData>("ImageAssetData")
                .BinLoad<&sky::aurora::ImageAssetData::Load>()
                .BinSave<&sky::aurora::ImageAssetData::Save>();

            auto *manager = AssetManager::Get();
            manager->RegisterAssetHandler<sky::aurora::Mesh>();
            manager->RegisterAssetHandler<sky::aurora::Material>();
            manager->RegisterAssetHandler<sky::aurora::Texture>();
        }

        static void RegisterComponents()
        {
            auto       *factory = ComponentFactory::Get();
            const std::string group = "Aurora";
            factory->RegisterComponent<sky::aurora::StaticMeshComponent>(group);
            factory->RegisterComponent<sky::aurora::DirectLightComponent>(group);
            factory->RegisterComponent<sky::aurora::PointLightComponent>(group);
            factory->RegisterComponent<sky::aurora::SpotLightComponent>(group);
            factory->RegisterComponent<sky::aurora::CameraComponent>(group);
        }

        void AuroraReflection(sky::SerializationContext *context)
        {
            static bool reflected = false;
            if (reflected) {
                return;
            }
            reflected = true;

            ReflectSceneTypes(context);
            ReflectAssetTypes(context);

            sky::aurora::StaticMeshComponent::Reflect(context);
            sky::aurora::DirectLightComponent::Reflect(context);
            sky::aurora::PointLightComponent::Reflect(context);
            sky::aurora::SpotLightComponent::Reflect(context);
            sky::aurora::CameraComponent::Reflect(context);
            RegisterComponents();
        }

    } // namespace aurora
} // namespace sky
