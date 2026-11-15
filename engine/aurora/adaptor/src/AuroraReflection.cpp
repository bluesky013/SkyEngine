//
// Runtime component metadata; persistent schemas are owned by Aurora.Metadata.
//

#include <aurora/adaptor/AuroraReflection.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/LodGroupAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/adaptor/components/CameraComponent.h>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/EnvironmentComponent.h>
#include <aurora/adaptor/components/LodGroupComponent.h>
#include <aurora/adaptor/components/PointLightComponent.h>
#include <aurora/adaptor/components/SkeletalMeshComponent.h>
#include <aurora/adaptor/components/SpotLightComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/scene/SceneTypes.h>

#include <framework/serialization/Any.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>

#include <string>

namespace sky {
    void aurora::EnvironmentComponent::Reflect(SerializationContext *context)
    {
        context->Register<aurora::EnvironmentComponent>("EnvironmentComponent")
            .Member<&aurora::EnvironmentComponent::SetDiffuseIrradiance, &aurora::EnvironmentComponent::GetDiffuseIrradiance>("diffuseIrradiance")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<aurora::Texture>::ASSET_TYPE))
            .Member<&aurora::EnvironmentComponent::SetSpecularEnvironment, &aurora::EnvironmentComponent::GetSpecularEnvironment>(
                "specularEnvironment")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<aurora::Texture>::ASSET_TYPE))
            .Member<&aurora::EnvironmentComponent::SetBrdfIntegration, &aurora::EnvironmentComponent::GetBrdfIntegration>("brdfIntegration")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<aurora::Texture>::ASSET_TYPE))
            .Member<&aurora::EnvironmentComponent::SetIntensity, &aurora::EnvironmentComponent::GetIntensity>("intensity")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.f))
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MAX), Any(64.f))
            .Member<&aurora::EnvironmentComponent::SetExposure, &aurora::EnvironmentComponent::GetExposure>("exposure")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(-16.f))
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MAX), Any(16.f))
            .Member<&aurora::EnvironmentComponent::SetEnabled, &aurora::EnvironmentComponent::GetEnabled>("enabled")
            .Member<&aurora::EnvironmentComponent::SetHdr, &aurora::EnvironmentComponent::GetHdr>("hdr");
    }

    void aurora::SkeletalMeshComponent::Reflect(SerializationContext *context)
    {
        using namespace aurora;
        context->Register<SkeletalMeshComponent>("SkeletalMeshComponent")
            .Member<&SkeletalMeshComponent::SetMeshUuid, &SkeletalMeshComponent::GetMeshUuid>("mesh")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Mesh>::ASSET_TYPE))
            .Member<&SkeletalMeshComponent::SetSkinUuid, &SkeletalMeshComponent::GetSkinUuid>("skin")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Skin>::ASSET_TYPE))
            .Member<&SkeletalMeshComponent::SetSkeletonUuid, &SkeletalMeshComponent::GetSkeletonUuid>("skeleton")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Skeleton>::ASSET_TYPE))
            .Member<&SkeletalMeshComponent::SetMaterialUuid, &SkeletalMeshComponent::GetMaterialUuid>("material")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Material>::ASSET_TYPE))
            .Member<&SkeletalMeshComponent::SetPoseMode, &SkeletalMeshComponent::GetPoseMode>("pose")
            .Member<&SkeletalMeshComponent::SetCastShadow, &SkeletalMeshComponent::GetCastShadow>("castShadow")
            .Member<&SkeletalMeshComponent::SetReceiveShadow, &SkeletalMeshComponent::GetReceiveShadow>("receiveShadow");
    }

    void aurora::StaticMeshComponent::Reflect(SerializationContext *context)
    {

        context->Register<aurora::StaticMeshComponent>("StaticMeshComponent")
            .Member<&aurora::StaticMeshComponent::SetMeshUuid, &aurora::StaticMeshComponent::GetMeshUuid>("mesh")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<aurora::Mesh>::ASSET_TYPE))
            .Member<&aurora::StaticMeshComponent::SetMaterialUuid, &aurora::StaticMeshComponent::GetMaterialUuid>("material")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<aurora::Material>::ASSET_TYPE))
            .Member<&aurora::StaticMeshComponent::SetCastShadow, &aurora::StaticMeshComponent::GetCastShadow>("castShadow")
            .Member<&aurora::StaticMeshComponent::SetReceiveShadow, &aurora::StaticMeshComponent::GetReceiveShadow>("receiveShadow");
    }

    void aurora::LodGroupComponent::Reflect(SerializationContext *context)
    {

        context->Register<aurora::LodGroupComponent>("LodGroupComponent")
            .Member<&aurora::LodGroupComponent::SetLodGroupUuid, &aurora::LodGroupComponent::GetLodGroupUuid>("lodGroup")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<aurora::LodGroup>::ASSET_TYPE))
            .Member<&aurora::LodGroupComponent::SetCastShadow, &aurora::LodGroupComponent::GetCastShadow>("castShadow")
            .Member<&aurora::LodGroupComponent::SetReceiveShadow, &aurora::LodGroupComponent::GetReceiveShadow>("receiveShadow");
    }

    void aurora::LodGroupComponent::OnAssetLoaded(const Uuid & /*uuid*/, const std::string_view & /*type*/)
    {
        // Device-side lod selection/build is a follow-up; the component tracks
        // the asset handle here.
    }

    void aurora::CameraComponent::Reflect(SerializationContext *context)
    {

        context->Register<aurora::CameraComponent>("CameraComponent")
            .Member<&aurora::CameraComponent::SetFov, &aurora::CameraComponent::GetFov>("fov")
            .Member<&aurora::CameraComponent::SetNearZ, &aurora::CameraComponent::GetNearZ>("nearZ")
            .Member<&aurora::CameraComponent::SetFarZ, &aurora::CameraComponent::GetFarZ>("farZ");
    }

    void aurora::DirectLightComponent::Reflect(SerializationContext *context)
    {

        context->Register<aurora::DirectLightComponent>("DirectLightComponent")
            .Member<&aurora::DirectLightComponent::SetColor, &aurora::DirectLightComponent::GetColor>("color")
            .Member<&aurora::DirectLightComponent::SetIntensity, &aurora::DirectLightComponent::GetIntensity>("intensity")
            .Member<&aurora::DirectLightComponent::SetCastShadow, &aurora::DirectLightComponent::GetCastShadow>("castShadow");
    }

    void aurora::PointLightComponent::Reflect(SerializationContext *context)
    {

        context->Register<aurora::PointLightComponent>("PointLightComponent")
            .Member<&aurora::PointLightComponent::SetColor, &aurora::PointLightComponent::GetColor>("color")
            .Member<&aurora::PointLightComponent::SetIntensity, &aurora::PointLightComponent::GetIntensity>("intensity")
            .Member<&aurora::PointLightComponent::SetRange, &aurora::PointLightComponent::GetRange>("range");
    }

    void aurora::SpotLightComponent::Reflect(SerializationContext *context)
    {

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

            context->Register<sky::aurora::Bounds>("Bounds").Member<&sky::aurora::Bounds::worldBounds>("worldBounds");

            context->Register<sky::aurora::WorldInfo>("WorldInfo").Member<&sky::aurora::WorldInfo::world>("world");

            context->Register<sky::aurora::LightType>("LightType")
                .Enum(sky::aurora::LightType::DIRECTIONAL, "DIRECTIONAL")
                .Enum(sky::aurora::LightType::POINT, "POINT")
                .Enum(sky::aurora::LightType::SPOT, "SPOT");

            context->Register<sky::aurora::Light>("Light")
                .Member<&sky::aurora::Light::type>("type")
                .Member<&sky::aurora::Light::color>("color")
                .Member<&sky::aurora::Light::intensity>("intensity")
                .Member<&sky::aurora::Light::range>("range")
                .Member<&sky::aurora::Light::innerConeAngle>("innerConeAngle")
                .Member<&sky::aurora::Light::outerConeAngle>("outerConeAngle");
        }

        static void RegisterComponents()
        {
            auto             *factory = ComponentFactory::Get();
            const std::string group   = "Aurora";
            factory->RegisterComponent<sky::aurora::StaticMeshComponent>(group);
            factory->RegisterComponent<sky::aurora::SkeletalMeshComponent>(group);
            factory->RegisterComponent<sky::aurora::LodGroupComponent>(group);
            factory->RegisterComponent<sky::aurora::DirectLightComponent>(group);
            factory->RegisterComponent<sky::aurora::EnvironmentComponent>(group);
            factory->RegisterComponent<sky::aurora::PointLightComponent>(group);
            factory->RegisterComponent<sky::aurora::SpotLightComponent>(group);
            factory->RegisterComponent<sky::aurora::CameraComponent>(group);
        }

        void RegisterAuroraRuntimeTypes(sky::SerializationContext *context)
        {
            ReflectSceneTypes(context);

            sky::aurora::StaticMeshComponent::Reflect(context);
            sky::aurora::SkeletalMeshComponent::Reflect(context);
            sky::aurora::LodGroupComponent::Reflect(context);
            sky::aurora::DirectLightComponent::Reflect(context);
            sky::aurora::EnvironmentComponent::Reflect(context);
            sky::aurora::PointLightComponent::Reflect(context);
            sky::aurora::SpotLightComponent::Reflect(context);
            sky::aurora::CameraComponent::Reflect(context);
            RegisterComponents();
        }

    } // namespace aurora
} // namespace sky
