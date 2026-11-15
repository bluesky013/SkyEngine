//
// Created on 2026/11/15.
//

#include <aurora/adaptor/components/AuroraComponentData.h>
#include <core/archive/MemoryStreamArchive.h>
#include <core/hash/Hash.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/WorldAsset.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using namespace sky;
using namespace sky::aurora;

namespace {
    Uuid ComponentId(const char *name)
    {
        return Uuid::CreateWithSeed(Fnv1a32(name));
    }

    template <typename T>
    void WriteComponent(JsonOutputArchive &archive, const char *name, const T &data)
    {
        archive.StartObject();
        archive.Key("type");
        archive.SaveValue(ComponentId(name).ToString());
        archive.Key("data");
        archive.SaveValueObject(data);
        archive.EndObject();
    }
} // namespace

TEST(AuroraWorldMetadata, PersistentSchemasAreAvailableWithoutRuntimeCreators)
{
    auto            *factory = ComponentFactory::Get();
    const std::array names{
        std::pair{"StaticMeshComponent", "StaticMeshComponentData"}, std::pair{"SkeletalMeshComponent", "SkeletalMeshComponentData"},
        std::pair{"LodGroupComponent", "LodGroupComponentData"},     std::pair{"PointLightComponent", "PointLightData"},
        std::pair{"DirectLightComponent", "DirectLightData"},        std::pair{"SpotLightComponent", "SpotLightData"},
        std::pair{"CameraComponent", "CameraComponentData"},         std::pair{"EnvironmentComponent", "EnvironmentData"}};
    for (const auto &[component, data] : names) {
        SCOPED_TRACE(component);
        const auto  id   = ComponentId(component);
        const auto *type = SerializationContext::Get()->FindType(data);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(factory->GetPersistentDataType(id), type->info);
        EXPECT_EQ(factory->Find(id), nullptr);
        EXPECT_EQ(SerializationContext::Get()->FindType(component), nullptr);
    }
}

TEST(AuroraWorldMetadata, CookAndBinaryRoundTripPreserveComponentSchemasAndAssetReferences)
{
    const auto       mesh     = Uuid::Create();
    const auto       material = Uuid::Create();
    const auto       skin     = Uuid::Create();
    const auto       skeleton = Uuid::Create();
    const std::array roots{std::pair{mesh, "AuroraMesh"}, std::pair{material, "AuroraMaterial"}, std::pair{skin, "AuroraSkin"},
                           std::pair{skeleton, "AuroraSkeleton"}};
    OMemoryArchive   text;
    {
        JsonOutputArchive json(text);
        json.StartObject();
        json.Key("assetRoots");
        json.StartArray();
        for (const auto &[id, type] : roots) {
            json.StartObject();
            json.Key("uuid");
            json.SaveValue(id.ToString());
            json.Key("type");
            json.SaveValue(type);
            json.EndObject();
        }
        json.EndArray();
        json.Key("subSystems");
        json.StartArray();
        json.EndArray();
        json.Key("actors");
        json.StartArray();
        json.StartObject();
        json.Key("uuid");
        json.SaveValue(Uuid::Create().ToString());
        json.Key("name");
        json.SaveValue("Offline Aurora component schemas");
        json.Key("components");
        json.StartArray();
        StaticMeshComponentData staticMesh;
        staticMesh.mesh     = mesh;
        staticMesh.material = material;
        WriteComponent(json, "StaticMeshComponent", staticMesh);
        SkeletalMeshComponentData skeletalMesh;
        skeletalMesh.mesh     = mesh;
        skeletalMesh.material = material;
        skeletalMesh.skin     = skin;
        skeletalMesh.skeleton = skeleton;
        WriteComponent(json, "SkeletalMeshComponent", skeletalMesh);
        WriteComponent(json, "LodGroupComponent", LodGroupComponentData{});
        WriteComponent(json, "PointLightComponent", PointLightData{});
        WriteComponent(json, "DirectLightComponent", DirectLightData{});
        WriteComponent(json, "SpotLightComponent", SpotLightData{});
        WriteComponent(json, "CameraComponent", CameraComponentData{});
        WriteComponent(json, "EnvironmentComponent", EnvironmentData{});
        json.EndArray();
        json.EndObject();
        json.EndArray();
        json.EndObject();
    }
    IMemoryArchive source(text.Data(), text.Size());
    WorldAssetData cooked;
    std::string    error;
    ASSERT_TRUE(CookWorldJson(source, cooked, error)) << error;
    ASSERT_EQ(cooked.actors.size(), 1U);
    ASSERT_EQ(cooked.actors.front().components.size(), 8U);
    ASSERT_EQ(cooked.roots.size(), roots.size());
    OMemoryArchive      bytes;
    BinaryOutputArchive encoded(bytes);
    cooked.Save(encoded);
    ASSERT_TRUE(encoded.IsValid());
    IMemoryArchive     input(bytes.Data(), bytes.Size());
    BinaryInputArchive decoded(input);
    WorldAssetData     reloaded;
    reloaded.Load(decoded);
    ASSERT_TRUE(decoded.IsValid());
    ASSERT_TRUE(reloaded.IsLoadValid()) << reloaded.GetLoadError();
    ASSERT_EQ(reloaded.actors.size(), 1U);
    ASSERT_EQ(reloaded.actors.front().components.size(), 8U);
    for (size_t i = 0; i < cooked.actors.front().components.size(); ++i) {
        const auto &before = cooked.actors.front().components[i];
        const auto &after  = reloaded.actors.front().components[i];
        EXPECT_EQ(after.componentType, before.componentType);
        EXPECT_EQ(after.dataType, before.dataType);
        EXPECT_EQ(after.payload, before.payload);
        EXPECT_EQ(ComponentFactory::Get()->Find(after.componentType), nullptr);
    }
    for (const auto &[id, type] : roots) {
        EXPECT_TRUE(
            std::any_of(reloaded.roots.begin(), reloaded.roots.end(), [&](const auto &root) { return root.uuid == id && root.type == type; }));
    }
    WorldPtr runtime;
    EXPECT_FALSE(InstantiateWorldAsset(reloaded, runtime, error));
    EXPECT_EQ(runtime, nullptr);
}
