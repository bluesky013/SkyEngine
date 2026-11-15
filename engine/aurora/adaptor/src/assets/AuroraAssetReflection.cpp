//
// Created on 2026/11/15.
//

#include <aurora/adaptor/assets/AuroraAssetReflection.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/LodGroupAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/adaptor/assets/MaterialReflection.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/adaptor/components/AuroraComponentData.h>
#include <core/hash/Fnv1a.h>
#include <core/shapes/Bounds.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>

namespace sky::aurora {
    namespace {
        template <class Data>
        void RegisterPersistentComponent(const char *name)
        {
            const bool registered =
                ComponentFactory::Get()->RegisterPersistentDataType(Uuid::CreateWithSeed(Fnv1a32(name)), TypeInfoObj<Data>::Get()->RtInfo());
            SKY_ASSERT(registered);
        }
    } // namespace

    void RegisterAuroraAssetTypes(SerializationContext *context)
    {
        context->Register<BoundingBoxSphere>("BoundingBoxSphere")
            .Member<&BoundingBoxSphere::center>("center")
            .Member<&BoundingBoxSphere::extent>("extent")
            .Member<&BoundingBoxSphere::radius>("radius");

        context->Register<sky::aurora::MeshAssetData>("MeshAssetData")
            .BinLoad<&sky::aurora::MeshAssetData::Load>()
            .BinSave<&sky::aurora::MeshAssetData::Save>();

        context->Register<sky::aurora::SkeletonAssetData>("SkeletonAssetData")
            .BinLoad<&sky::aurora::SkeletonAssetData::Load>()
            .BinSave<&sky::aurora::SkeletonAssetData::Save>();

        context->Register<sky::aurora::SkinAssetData>("SkinAssetData")
            .BinLoad<&sky::aurora::SkinAssetData::Load>()
            .BinSave<&sky::aurora::SkinAssetData::Save>();

        sky::aurora::RegisterMaterialReflection(context);

        context->Register<sky::aurora::MaterialInstanceAssetData>("MaterialInstanceAssetData")
            .BinLoad<&sky::aurora::MaterialInstanceAssetData::Load>()
            .BinSave<&sky::aurora::MaterialInstanceAssetData::Save>();

        context->Register<sky::aurora::ImageAssetData>("ImageAssetData")
            .BinLoad<&sky::aurora::ImageAssetData::Load>()
            .BinSave<&sky::aurora::ImageAssetData::Save>();

        context->Register<sky::aurora::LodGroupAssetData>("LodGroupAssetData")
            .BinLoad<&sky::aurora::LodGroupAssetData::Load>()
            .BinSave<&sky::aurora::LodGroupAssetData::Save>();

        auto *manager = AssetManager::Get();
        manager->RegisterAssetHandler<sky::aurora::Mesh>();
        manager->RegisterAssetHandler<sky::Skeleton>();
        manager->RegisterAssetHandler<sky::aurora::Skin>();
        manager->RegisterAssetHandler<sky::aurora::Material>();
        manager->RegisterAssetHandler<sky::aurora::MaterialInstance>();
        manager->RegisterAssetHandler<sky::aurora::Texture>();
        manager->RegisterAssetHandler<sky::aurora::LodGroup>();

        context->Register<EnvironmentData>("EnvironmentData")
            .Member<&EnvironmentData::diffuseIrradiance>("diffuseIrradiance")
            .Member<&EnvironmentData::specularEnvironment>("specularEnvironment")
            .Member<&EnvironmentData::brdfIntegration>("brdfIntegration")
            .Member<&EnvironmentData::intensity>("intensity")
            .Member<&EnvironmentData::exposure>("exposure")
            .Member<&EnvironmentData::enabled>("enabled")
            .Member<&EnvironmentData::hdr>("hdr");
        context->Register<SkeletalPoseMode>("SkeletalPoseMode")
            .Enum(SkeletalPoseMode::ReferencePose, "ReferencePose")
            .Enum(SkeletalPoseMode::SameActorAnimation, "SameActorAnimation");
        context->Register<SkeletalMeshComponentData>("SkeletalMeshComponentData")
            .Member<&SkeletalMeshComponentData::mesh>("mesh")
            .Member<&SkeletalMeshComponentData::skin>("skin")
            .Member<&SkeletalMeshComponentData::skeleton>("skeleton")
            .Member<&SkeletalMeshComponentData::material>("material")
            .Member<&SkeletalMeshComponentData::pose>("pose")
            .Member<&SkeletalMeshComponentData::castShadow>("castShadow")
            .Member<&SkeletalMeshComponentData::receiveShadow>("receiveShadow");
        context->Register<StaticMeshComponentData>("StaticMeshComponentData")
            .Member<&StaticMeshComponentData::mesh>("mesh")
            .Member<&StaticMeshComponentData::material>("material")
            .Member<&StaticMeshComponentData::castShadow>("castShadow")
            .Member<&StaticMeshComponentData::receiveShadow>("receiveShadow");
        context->Register<LodGroupComponentData>("LodGroupComponentData")
            .Member<&LodGroupComponentData::lodGroup>("lodGroup")
            .Member<&LodGroupComponentData::castShadow>("castShadow")
            .Member<&LodGroupComponentData::receiveShadow>("receiveShadow");
        context->Register<CameraComponentData>("CameraComponentData")
            .Member<&CameraComponentData::fov>("fov")
            .Member<&CameraComponentData::nearZ>("nearZ")
            .Member<&CameraComponentData::farZ>("farZ");
        context->Register<DirectLightData>("DirectLightData")
            .Member<&DirectLightData::color>("color")
            .Member<&DirectLightData::intensity>("intensity")
            .Member<&DirectLightData::castShadow>("castShadow");
        context->Register<PointLightData>("PointLightData")
            .Member<&PointLightData::color>("color")
            .Member<&PointLightData::intensity>("intensity")
            .Member<&PointLightData::range>("range");
        context->Register<SpotLightData>("SpotLightData")
            .Member<&SpotLightData::color>("color")
            .Member<&SpotLightData::intensity>("intensity")
            .Member<&SpotLightData::range>("range")
            .Member<&SpotLightData::innerConeAngle>("innerConeAngle")
            .Member<&SpotLightData::outerConeAngle>("outerConeAngle");

        RegisterPersistentComponent<StaticMeshComponentData>("StaticMeshComponent");
        RegisterPersistentComponent<SkeletalMeshComponentData>("SkeletalMeshComponent");
        RegisterPersistentComponent<LodGroupComponentData>("LodGroupComponent");
        RegisterPersistentComponent<CameraComponentData>("CameraComponent");
        RegisterPersistentComponent<DirectLightData>("DirectLightComponent");
        RegisterPersistentComponent<PointLightData>("PointLightComponent");
        RegisterPersistentComponent<SpotLightData>("SpotLightComponent");
        RegisterPersistentComponent<EnvironmentData>("EnvironmentComponent");
    }
} // namespace sky::aurora
