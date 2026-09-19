//
// Aurora -> framework reflection / asset / component bridge implementation.
//

#include <aurora/adaptor/AuroraReflection.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/components/AuroraCameraComponent.h>
#include <aurora/adaptor/components/AuroraLightComponent.h>
#include <aurora/adaptor/components/AuroraStaticMeshComponent.h>
#include <aurora/scene/SceneTypes.h>

#include <core/shapes/AABB.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/Any.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>

#include <string>

namespace sky {

    void AuroraStaticMeshComponent::Reflect(SerializationContext *context)
    {
        context->Register<AuroraStaticMeshData>("AuroraStaticMeshData")
            .Member<&AuroraStaticMeshData::mesh>("mesh")
            .Member<&AuroraStaticMeshData::material>("material")
            .Member<&AuroraStaticMeshData::castShadow>("castShadow")
            .Member<&AuroraStaticMeshData::receiveShadow>("receiveShadow");

        context->Register<AuroraStaticMeshComponent>("AuroraStaticMeshComponent")
            .Member<&AuroraStaticMeshComponent::SetMeshUuid, &AuroraStaticMeshComponent::GetMeshUuid>("mesh")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<sky::aurora::Mesh>::ASSET_TYPE))
            .Member<&AuroraStaticMeshComponent::SetMaterialUuid, &AuroraStaticMeshComponent::GetMaterialUuid>("material")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<sky::aurora::Material>::ASSET_TYPE));
    }

    void AuroraStaticMeshComponent::OnAssetLoaded(const Uuid & /*uuid*/, const std::string_view & /*type*/)
    {
        // Device-side resource build (mesh renderer / material binding) is a
        // follow-up; the component tracks the asset handles here.
    }

    void AuroraLightComponent::Reflect(SerializationContext *context)
    {
        context->Register<sky::aurora::LightType>("AuroraLightType")
            .Enum(sky::aurora::LightType::DIRECTIONAL, "DIRECTIONAL")
            .Enum(sky::aurora::LightType::POINT, "POINT")
            .Enum(sky::aurora::LightType::SPOT, "SPOT");

        context->Register<sky::aurora::Light>("AuroraLight")
            .Member<&sky::aurora::Light::type>("type")
            .Member<&sky::aurora::Light::color>("color")
            .Member<&sky::aurora::Light::intensity>("intensity")
            .Member<&sky::aurora::Light::direction>("direction")
            .Member<&sky::aurora::Light::position>("position")
            .Member<&sky::aurora::Light::range>("range")
            .Member<&sky::aurora::Light::innerConeAngle>("innerConeAngle")
            .Member<&sky::aurora::Light::outerConeAngle>("outerConeAngle");

        context->Register<AuroraLightComponent>("AuroraLightComponent");
    }

    void AuroraCameraComponent::Reflect(SerializationContext *context)
    {
        context->Register<AuroraCameraData>("AuroraCameraData")
            .Member<&AuroraCameraData::fov>("fov")
            .Member<&AuroraCameraData::nearZ>("nearZ")
            .Member<&AuroraCameraData::farZ>("farZ");

        context->Register<AuroraCameraComponent>("AuroraCameraComponent")
            .Member<&AuroraCameraComponent::SetFov, &AuroraCameraComponent::GetFov>("fov")
            .Member<&AuroraCameraComponent::SetNearZ, &AuroraCameraComponent::GetNearZ>("nearZ")
            .Member<&AuroraCameraComponent::SetFarZ, &AuroraCameraComponent::GetFarZ>("farZ");
    }

    namespace aurora {

        static void ReflectSceneTypes(SerializationContext *context)
        {
            context->Register<AABB>("AABB")
                .Member<&AABB::min>("min")
                .Member<&AABB::max>("max");

            context->Register<sky::aurora::Bounds>("AuroraBounds")
                .Member<&sky::aurora::Bounds::worldBounds>("worldBounds");

            context->Register<sky::aurora::WorldInfo>("AuroraWorldInfo")
                .Member<&sky::aurora::WorldInfo::world>("world");
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
            auto *factory = ComponentFactory::Get();
            const std::string group = "Aurora";
            factory->RegisterComponent<AuroraStaticMeshComponent>(group);
            factory->RegisterComponent<AuroraLightComponent>(group);
            factory->RegisterComponent<AuroraCameraComponent>(group);
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

            AuroraStaticMeshComponent::Reflect(context);
            AuroraLightComponent::Reflect(context);
            AuroraCameraComponent::Reflect(context);
            RegisterComponents();
        }

    } // namespace aurora
} // namespace sky
