#include <framework/world/WorldAsset.h>

#include <core/archive/MemoryStreamArchive.h>
#include <core/math/Matrix4.h>
#include <core/math/Quaternion.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/SimpleRotateComponent.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <list>
#include <map>
#include <span>
#include <sstream>

using namespace sky;

namespace {
    struct WorldBinaryLeaf {
        int32_t     number = -17;
        std::string text   = "typed leaf";
    };

    struct WorldBinaryFields {
        Vector2                      uv{1, 2};
        Vector3                      position{3, 4, 5};
        Vector4                      color{6, 7, 8, 9};
        Quaternion                   rotation;
        Matrix4                      matrix;
        Uuid                         reference = Uuid::Create();
        Name                         name      = Name("world binary name");
        std::string                  text      = "UTF-8 text";
        std::vector<uint32_t>        numbers{1, 2, 3};
        std::list<std::string>       labels{"first", "second"};
        std::vector<WorldBinaryLeaf> leaves{{}, {23, "second leaf"}};
        Any                          dynamic;
    };

    class WorldBinaryComponent : public ComponentAdaptor<WorldBinaryFields> {
    public:
        COMPONENT_RUNTIME_INFO(WorldBinaryComponent)
        WorldBinaryFields &MutableData()
        {
            return data;
        }
    };

    class WorldBinaryCountedComponent : public ComponentAdaptor<WorldBinaryLeaf> {
    public:
        COMPONENT_RUNTIME_INFO(WorldBinaryCountedComponent)
        WorldBinaryCountedComponent()
        {
            ++constructions;
        }
        inline static uint32_t constructions = 0;
    };

    class WorldBinaryNoStaticSchemaComponent : public ComponentBase {
    public:
        COMPONENT_RUNTIME_INFO(WorldBinaryNoStaticSchemaComponent)
    };

    class WorldBinaryRequiredComponent : public ComponentAdaptor<WorldBinaryLeaf> {
    public:
        COMPONENT_RUNTIME_INFO(WorldBinaryRequiredComponent)
    };

    class WorldBinaryFailingComponent : public ComponentAdaptor<WorldBinaryLeaf> {
    public:
        COMPONENT_RUNTIME_INFO(WorldBinaryFailingComponent)
        bool TryAttachToWorld(std::string &error) override
        {
            error = "Binary component attachment rejected";
            return false;
        }
        void OnDetachFromWorld() override
        {
            ++detachments;
        }
        inline static uint32_t detachments = 0;
    };

    struct WorldBinaryDynamicValue {
        WorldBinaryDynamicValue()
        {
            ++constructions;
        }
        int32_t                number        = 31;
        inline static uint32_t constructions = 0;
    };

    struct WorldBinaryNonDefaultValue {
        explicit WorldBinaryNonDefaultValue(int32_t value) : number(value)
        {
        }
        int32_t number;
    };

    struct WorldBinaryAnyData {
        Any value;
    };

    class WorldBinaryAnyComponent : public ComponentAdaptor<WorldBinaryAnyData> {
    public:
        COMPONENT_RUNTIME_INFO(WorldBinaryAnyComponent)
    };

    struct OtherPolymorphicBase {
        virtual ~OtherPolymorphicBase() = default;
        uint64_t marker                 = 0x12345678;
    };

    class WorldBinaryMultipleComponent : public OtherPolymorphicBase, public ComponentAdaptor<WorldBinaryLeaf> {
    public:
        COMPONENT_RUNTIME_INFO(WorldBinaryMultipleComponent)
        void OnAttachToWorld() override
        {
            sawSubsystem = GetActor()->GetWorld()->GetSubSystem(Name("WorldBinaryTest")) != nullptr;
        }
        bool sawSubsystem = false;
    };

    struct WorldBinaryUnsupportedData {
        std::map<std::string, uint32_t> values;
    };
    class WorldBinaryUnsupportedComponent : public ComponentAdaptor<WorldBinaryUnsupportedData> {
    public:
        COMPONENT_RUNTIME_INFO(WorldBinaryUnsupportedComponent)
    };

    struct WorldBinaryConfig {
        uint32_t           count = 3;
        Vector3            gravity{0, -10, 0};
        Name               label = Name("subsystem config");
        std::vector<float> values{1.f, 2.f};
    };
    struct WorldBinarySubSystem : IWorldSubSystem {
        WorldBinaryConfig config;
    };

    class WorldAssetTypes : public testing::Environment {
    public:
        void SetUp() override
        {
            auto *context = SerializationContext::Get();
            context->Register<WorldBinaryLeaf>("WorldBinaryLeaf").Member<&WorldBinaryLeaf::number>("number").Member<&WorldBinaryLeaf::text>("text");
            context->Register<WorldBinaryFields>("WorldBinaryFields")
                .Member<&WorldBinaryFields::uv>("uv")
                .Member<&WorldBinaryFields::position>("position")
                .Member<&WorldBinaryFields::color>("color")
                .Member<&WorldBinaryFields::rotation>("rotation")
                .Member<&WorldBinaryFields::matrix>("matrix")
                .Member<&WorldBinaryFields::reference>("reference")
                .Member<&WorldBinaryFields::name>("name")
                .Member<&WorldBinaryFields::text>("text")
                .Member<&WorldBinaryFields::numbers>("numbers")
                .Member<&WorldBinaryFields::labels>("labels")
                .Member<&WorldBinaryFields::leaves>("leaves")
                .Member<&WorldBinaryFields::dynamic>("dynamic");
            context->Register<WorldBinaryComponent>("WorldBinaryComponent");
            context->Register<WorldBinaryCountedComponent>("WorldBinaryCountedComponent");
            context->Register<WorldBinaryNoStaticSchemaComponent>("WorldBinaryNoStaticSchemaComponent");
            context->Register<WorldBinaryRequiredComponent>("WorldBinaryRequiredComponent");
            context->Register<WorldBinaryFailingComponent>("WorldBinaryFailingComponent");
            context->Register<WorldBinaryDynamicValue>("WorldBinaryDynamicValue").Member<&WorldBinaryDynamicValue::number>("number");
            context->Register<WorldBinaryNonDefaultValue>("WorldBinaryNonDefaultValue")
                .Constructor<int32_t>()
                .Member<&WorldBinaryNonDefaultValue::number>("number");
            context->Register<WorldBinaryAnyData>("WorldBinaryAnyData").Member<&WorldBinaryAnyData::value>("value");
            context->Register<WorldBinaryAnyComponent>("WorldBinaryAnyComponent");
            context->Register<WorldBinaryMultipleComponent>("WorldBinaryMultipleComponent");
            context->Register<WorldBinaryUnsupportedData>("WorldBinaryUnsupportedData").Member<&WorldBinaryUnsupportedData::values>("values");
            context->Register<WorldBinaryUnsupportedComponent>("WorldBinaryUnsupportedComponent");
            context->Register<WorldBinaryConfig>("WorldBinaryConfig")
                .Member<&WorldBinaryConfig::count>("count")
                .Member<&WorldBinaryConfig::gravity>("gravity")
                .Member<&WorldBinaryConfig::label>("label")
                .Member<&WorldBinaryConfig::values>("values");
            auto *components = ComponentFactory::Get();
            components->RegisterComponent<WorldBinaryComponent>("WorldAssetTest");
            components->RegisterComponent<WorldBinaryCountedComponent>("WorldAssetTest");
            components->RegisterComponent<WorldBinaryNoStaticSchemaComponent>("WorldAssetTest");
            components->RegisterComponent<WorldBinaryRequiredComponent>("WorldAssetTest", {Name("WorldBinaryTest")});
            components->RegisterComponent<WorldBinaryFailingComponent>("WorldAssetTest");
            components->RegisterComponent<WorldBinaryAnyComponent>("WorldAssetTest");
            components->RegisterComponent<WorldBinaryMultipleComponent>("WorldAssetTest");
            components->RegisterComponent<WorldBinaryUnsupportedComponent>("WorldAssetTest");
        }
    };

    const auto *worldAssetTypes = testing::AddGlobalTestEnvironment(new WorldAssetTypes());

    class WorldAssetTest : public testing::Test {
    protected:
        void SetUp() override
        {
            auto &registry = WorldSubSystemRegistry::Get();
            ASSERT_TRUE(registry.Register(Name("WorldBinaryTest"), {
                                                                       [](World &, const Any &config) -> std::unique_ptr<IWorldSubSystem> {
                                                                           auto result    = std::make_unique<WorldBinarySubSystem>();
                                                                           result->config = *config.GetAsConst<WorldBinaryConfig>();
                                                                           return result;
                                                                       },
                                                                       TypeInfoObj<WorldBinaryConfig>::Get()->RtInfo(),
                                                                       [] { return Any(std::in_place_type<WorldBinaryConfig>); },
                                                                       [](const Any &config, std::string &error) {
                                                                           if (!config.GetAsConst<WorldBinaryConfig>() ||
                                                                               config.GetAsConst<WorldBinaryConfig>()->count > 1024) {
                                                                               error = "Invalid test subsystem count";
                                                                               return false;
                                                                           }
                                                                           return true;
                                                                       },
                                                                       {},
                                                                   }));
        }
        void TearDown() override
        {
            WorldSubSystemRegistry::Get().Unregister(Name("WorldBinaryTest"));
        }

        std::vector<uint8_t> Encode(const WorldAssetData &data)
        {
            OMemoryArchive      stream;
            BinaryOutputArchive archive(stream);
            data.Save(archive);
            EXPECT_TRUE(archive.IsValid());
            const auto *bytes = reinterpret_cast<const uint8_t *>(stream.Data());
            return {bytes, bytes + stream.Size()};
        }
        bool Decode(std::span<const uint8_t> bytes, WorldAssetData &data)
        {
            IMemoryArchive     stream(bytes.data(), bytes.size());
            BinaryInputArchive archive(stream);
            data.Load(archive);
            return archive.IsValid() && data.IsLoadValid();
        }
        std::string Source(World &world)
        {
            OMemoryArchive    stream;
            JsonOutputArchive json(stream);
            world.SaveJson(json);
            return {stream.Data(), stream.Size()};
        }
        bool Cook(std::string_view text, WorldAssetData &data, std::string &error)
        {
            IMemoryArchive stream(text.data(), text.size());
            return CookWorldJson(stream, data, error);
        }
    };
} // namespace

TEST_F(WorldAssetTest, AnyRequiresAReflectedDefaultConstructorWithoutConstructingDuringValidation)
{
    const auto supportedType   = TypeInfoObj<WorldBinaryDynamicValue>::Get()->RtInfo()->registeredId;
    const auto unsupportedType = TypeInfoObj<WorldBinaryNonDefaultValue>::Get()->RtInfo()->registeredId;
    const auto source          = [](const Uuid &valueType) {
        std::ostringstream text;
        text << R"({"assetRoots":[],"subSystems":[],"actors":[{"uuid":")" << Uuid::Create().ToString() << R"(","name":"Any","components":[{"type":")"
             << TypeInfo<WorldBinaryAnyComponent>::RegisteredId().ToString() << R"(","data":{"classId":")"
             << TypeInfo<WorldBinaryAnyData>::RegisteredId().ToString() << R"(","elements":{"value":{"classId":")" << valueType.ToString()
             << R"(","elements":{"number":37}}}}}]}]})";
        return text.str();
    };
    WorldBinaryDynamicValue::constructions = 0;
    WorldAssetData cooked;
    std::string    error;
    ASSERT_TRUE(Cook(source(supportedType), cooked, error)) << error;
    ASSERT_TRUE(cooked.Validate(error)) << error;
    const auto     bytes = Encode(cooked);
    WorldAssetData loaded;
    ASSERT_TRUE(Decode(bytes, loaded)) << loaded.GetLoadError();
    EXPECT_EQ(WorldBinaryDynamicValue::constructions, 0u);
    WorldPtr world;
    ASSERT_TRUE(InstantiateWorldAsset(loaded, world, error)) << error;
    EXPECT_EQ(WorldBinaryDynamicValue::constructions, 1u);
    const auto *component = world->GetActors()[0]->GetComponent<WorldBinaryAnyComponent>();
    ASSERT_NE(component, nullptr);
    const auto *value = component->GetData().value.GetAsConst<WorldBinaryDynamicValue>();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->number, 37);

    WorldAssetData rejected;
    EXPECT_FALSE(Cook(source(unsupportedType), rejected, error));
    EXPECT_NE(error.find("no reflected default constructor"), std::string::npos);
    auto     badBytes     = bytes;
    uint32_t replacements = 0;
    for (auto it = badBytes.begin(); it != badBytes.end();) {
        it = std::search(it, badBytes.end(), std::begin(supportedType.data), std::end(supportedType.data));
        if (it == badBytes.end())
            break;
        std::copy(std::begin(unsupportedType.data), std::end(unsupportedType.data), it);
        it += sizeof(supportedType.data);
        ++replacements;
    }
    ASSERT_EQ(replacements, 2u);
    EXPECT_FALSE(Decode(badBytes, rejected));
    EXPECT_NE(rejected.GetLoadError().find("no reflected default constructor"), std::string::npos);
    EXPECT_TRUE(rejected.actors.empty());
    EXPECT_EQ(WorldBinaryDynamicValue::constructions, 1u);
}

TEST_F(WorldAssetTest, CookSaveLoadAndValidationDoNotConstructLiveComponents)
{
    const WorldPtr original = World::CreateWorld();
    auto          *actor    = original->CreateActor("Counted", false);
    ASSERT_NE(actor->AddComponent<WorldBinaryCountedComponent>(), nullptr);
    const auto source                          = Source(*original);
    WorldBinaryCountedComponent::constructions = 0;

    WorldAssetData cooked;
    std::string    error;
    ASSERT_TRUE(Cook(source, cooked, error)) << error;
    EXPECT_EQ(WorldBinaryCountedComponent::constructions, 0u);
    ASSERT_TRUE(cooked.Validate(error)) << error;
    EXPECT_EQ(WorldBinaryCountedComponent::constructions, 0u);
    const auto bytes = Encode(cooked);
    EXPECT_EQ(WorldBinaryCountedComponent::constructions, 0u);
    WorldAssetData loaded;
    ASSERT_TRUE(Decode(bytes, loaded)) << loaded.GetLoadError();
    ASSERT_TRUE(loaded.Validate(error)) << error;
    EXPECT_EQ(WorldBinaryCountedComponent::constructions, 0u);
    WorldAssetData captured;
    ASSERT_TRUE(CaptureWorldAsset(*original, captured, error)) << error;
    EXPECT_EQ(WorldBinaryCountedComponent::constructions, 0u);

    ASSERT_EQ(loaded.actors.size(), 1u);
    ASSERT_EQ(loaded.actors[0].components.size(), 1u);
    auto malformed = loaded;
    ASSERT_FALSE(malformed.actors[0].components[0].payload.empty());
    malformed.actors[0].components[0].payload.pop_back();
    EXPECT_FALSE(malformed.Validate(error));
    EXPECT_EQ(WorldBinaryCountedComponent::constructions, 0u);

    const auto unsupportedType                        = TypeInfoObj<WorldBinaryNoStaticSchemaComponent>::Get()->RtInfo()->registeredId;
    auto       unsupported                            = loaded;
    unsupported.actors[0].components[0].componentType = unsupportedType;
    EXPECT_FALSE(unsupported.Validate(error));
    EXPECT_NE(error.find("static persistence schema"), std::string::npos);
    auto       unsupportedSource = source;
    const auto componentId       = TypeInfoObj<WorldBinaryCountedComponent>::Get()->RtInfo()->registeredId.ToString();
    const auto typePosition      = unsupportedSource.find(componentId);
    ASSERT_NE(typePosition, std::string::npos);
    unsupportedSource.replace(typePosition, componentId.size(), unsupportedType.ToString());
    EXPECT_FALSE(Cook(unsupportedSource, unsupported, error));
    EXPECT_NE(error.find("static persistence schema"), std::string::npos);
    EXPECT_EQ(WorldBinaryCountedComponent::constructions, 0u);

    WorldPtr instantiated;
    ASSERT_TRUE(InstantiateWorldAsset(loaded, instantiated, error)) << error;
    EXPECT_EQ(WorldBinaryCountedComponent::constructions, 1u);
    ASSERT_EQ(instantiated->GetActors().size(), 1u);
    EXPECT_NE(instantiated->GetActors()[0]->GetComponent<WorldBinaryCountedComponent>(), nullptr);
}

TEST_F(WorldAssetTest, TypedFieldsRoundTripWithoutJsonPayload)
{
    WorldPtr original  = World::CreateWorld();
    auto    *actor     = original->CreateActor("Typed", false);
    auto    *component = actor->AddComponent<WorldBinaryComponent>();
    ASSERT_NE(component, nullptr);
    auto &fields        = component->MutableData();
    fields.matrix.v[3]  = 19.25f;
    fields.matrix.v[14] = -42.f;
    fields.rotation.x   = .25f;
    fields.rotation.w   = .75f;
    fields.dynamic      = Any(WorldBinaryLeaf{73, "dynamic value"});
    WorldAssetData data;
    std::string    error;
    ASSERT_TRUE(CaptureWorldAsset(*original, data, error)) << error;
    auto bytes = Encode(data);
    ASSERT_GT(bytes.size(), 12u);
    EXPECT_EQ(std::string(bytes.begin(), bytes.begin() + 4), "WRLD");
    EXPECT_EQ(bytes[4], 1u);
    EXPECT_EQ(bytes[5], 0u);
    EXPECT_EQ(std::string(bytes.begin(), bytes.end()).find("\"classId\""), std::string::npos);
    WorldAssetData restored;
    ASSERT_TRUE(Decode(bytes, restored)) << restored.GetLoadError();
    WorldPtr world;
    ASSERT_TRUE(InstantiateWorldAsset(restored, world, error)) << error;
    ASSERT_EQ(world->GetActors().size(), 1u);
    const auto *result = world->GetActors()[0]->GetComponent<WorldBinaryComponent>();
    ASSERT_NE(result, nullptr);
    const auto &actual = result->GetData();
    EXPECT_EQ(actual.reference, fields.reference);
    EXPECT_EQ(actual.name.GetStr(), fields.name.GetStr());
    EXPECT_EQ(actual.text, fields.text);
    EXPECT_EQ(actual.numbers, fields.numbers);
    EXPECT_EQ(actual.labels, fields.labels);
    ASSERT_EQ(actual.leaves.size(), 2u);
    EXPECT_EQ(actual.leaves[1].number, 23);
    EXPECT_EQ(actual.position.z, 5.f);
    EXPECT_EQ(actual.uv.y, 2.f);
    EXPECT_EQ(actual.color.w, 9.f);
    EXPECT_EQ(actual.matrix.v[3], 19.25f);
    EXPECT_EQ(actual.matrix.v[14], -42.f);
    EXPECT_EQ(actual.rotation.x, .25f);
    ASSERT_NE(actual.dynamic.GetAsConst<WorldBinaryLeaf>(), nullptr);
    EXPECT_EQ(actual.dynamic.GetAsConst<WorldBinaryLeaf>()->number, 73);
}

TEST_F(WorldAssetTest, JsonCookPreservesReverseOrderedThreeLevelHierarchyAndTypedConfig)
{
    WorldPtr original = World::CreateWorld();
    auto    *child    = original->CreateActor("Child");
    auto    *parent   = original->CreateActor("Parent");
    auto    *root     = original->CreateActor("Root");
    child->AddComponent<WorldBinaryMultipleComponent>();
    root->GetComponent<TransformComponent>()->SetLocalTranslation({10, 0, 0});
    parent->GetComponent<TransformComponent>()->SetLocalTranslation({2, 0, 0});
    parent->GetComponent<TransformComponent>()->SetParentPreserveLocal(root->GetComponent<TransformComponent>());
    child->GetComponent<TransformComponent>()->SetLocalTranslation({3, 0, 0});
    child->GetComponent<TransformComponent>()->SetParentPreserveLocal(parent->GetComponent<TransformComponent>());
    WorldBinaryConfig config;
    config.count = 7;
    original->GetMutableWorldDesc()->subSystems.push_back({Name("WorldBinaryTest"), Any(config), true});
    WorldAssetData cooked;
    std::string    error;
    ASSERT_TRUE(Cook(Source(*original), cooked, error)) << error;
    WorldAssetData loaded;
    ASSERT_TRUE(Decode(Encode(cooked), loaded)) << loaded.GetLoadError();
    WorldPtr restored;
    ASSERT_TRUE(InstantiateWorldAsset(loaded, restored, error)) << error;
    auto *restoredChild = restored->GetActorByUuid(child->GetUuid())->GetComponent<TransformComponent>();
    ASSERT_NE(restoredChild->GetParent(), nullptr);
    EXPECT_EQ(restoredChild->GetParent()->GetActor()->GetUuid(), parent->GetUuid());
    EXPECT_EQ(restoredChild->GetLocalTranslation().x, 3.f);
    EXPECT_EQ(restoredChild->GetWorldTransform().translation.x, 15.f);
    const auto *actual = static_cast<const WorldBinarySubSystem *>(restored->GetSubSystem(Name("WorldBinaryTest")));
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual->config.count, 7u);
    EXPECT_EQ(actual->config.gravity.y, -10.f);
    EXPECT_EQ(actual->config.label.GetStr(), "subsystem config");
    EXPECT_TRUE(restored->GetActorByUuid(child->GetUuid())->GetComponent<WorldBinaryMultipleComponent>()->sawSubsystem);
}

TEST_F(WorldAssetTest, MultipleInheritanceComponentUsesTypedFactoryAddressAdjustment)
{
    WorldPtr                       original  = World::CreateWorld();
    auto                          *actor     = original->CreateActor("Multiple", false);
    std::unique_ptr<ComponentBase> component = std::make_unique<WorldBinaryMultipleComponent>();
    ASSERT_TRUE(actor->AttachComponent(component));
    WorldAssetData data;
    std::string    error;
    ASSERT_TRUE(CaptureWorldAsset(*original, data, error)) << error;
    WorldPtr restored;
    ASSERT_TRUE(InstantiateWorldAsset(data, restored, error)) << error;
    auto *base = restored->GetActors()[0]->GetComponent(TypeInfo<WorldBinaryMultipleComponent>::RegisteredId());
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(static_cast<const WorldBinaryLeaf *>(base->GetPersistentData())->number, -17);
}

TEST_F(WorldAssetTest, InstantiationRejectsMissingRequiredSubsystemWithoutReplacingWorld)
{
    WorldPtr source = World::CreateWorld();
    source->CreateActor("Required", false)->AddComponent<WorldBinaryCountedComponent>();
    WorldAssetData data;
    std::string    error;
    ASSERT_TRUE(CaptureWorldAsset(*source, data, error)) << error;
    data.actors.front().components.front().componentType = TypeInfo<WorldBinaryRequiredComponent>::RegisteredId();
    ASSERT_TRUE(data.Validate(error)) << error;

    WorldPtr retained = World::CreateWorld();
    retained->CreateActor("Existing");
    WorldPtr result = retained;
    EXPECT_FALSE(InstantiateWorldAsset(data, result, error));
    EXPECT_EQ(result.Get(), retained.Get());
    EXPECT_NE(error.find("requires world subsystem WorldBinaryTest"), std::string::npos);
    ASSERT_EQ(result->GetActors().size(), 1u);
    EXPECT_EQ(result->GetActors().front()->GetName(), "Existing");
}

TEST_F(WorldAssetTest, InstantiationPropagatesAttachmentFailureAndRollsBackCandidate)
{
    WorldPtr source = World::CreateWorld();
    source->CreateActor("Failing", false)->AddComponent<WorldBinaryCountedComponent>();
    WorldAssetData data;
    std::string    error;
    ASSERT_TRUE(CaptureWorldAsset(*source, data, error)) << error;
    data.actors.front().components.front().componentType = TypeInfo<WorldBinaryFailingComponent>::RegisteredId();
    ASSERT_TRUE(data.Validate(error)) << error;

    WorldPtr       retained    = World::CreateWorld();
    WorldPtr       result      = retained;
    const uint32_t detachments = WorldBinaryFailingComponent::detachments;
    EXPECT_FALSE(InstantiateWorldAsset(data, result, error));
    EXPECT_EQ(result.Get(), retained.Get());
    EXPECT_NE(error.find("Binary component attachment rejected"), std::string::npos);
    EXPECT_EQ(WorldBinaryFailingComponent::detachments, detachments + 1);
}

TEST_F(WorldAssetTest, TruncatedOversizedVersionAndCountRecordsFailBeforePublishing)
{
    WorldPtr original = World::CreateWorld();
    original->CreateActor("Root");
    WorldAssetData data;
    std::string    error;
    ASSERT_TRUE(CaptureWorldAsset(*original, data, error));
    const auto good = Encode(data);
    for (size_t size = 0; size < good.size(); ++size) {
        WorldAssetData rejected;
        ASSERT_FALSE(Decode(std::span(good.data(), size), rejected)) << size;
        EXPECT_TRUE(rejected.actors.empty());
    }
    for (const auto offset : {4u, 8u, 16u}) {
        auto bad = good;
        std::fill_n(bad.begin() + offset, 4, 0xFF);
        WorldAssetData rejected;
        EXPECT_FALSE(Decode(bad, rejected));
        EXPECT_FALSE(rejected.GetLoadError().empty());
    }
    auto bad = data;
    std::fill_n(bad.actors[0].components[0].payload.begin() + 17, 4, 0xFF);
    WorldPtr preserved = original;
    EXPECT_FALSE(InstantiateWorldAsset(bad, preserved, error));
    EXPECT_EQ(preserved.Get(), original.Get());
}

TEST_F(WorldAssetTest, InvalidJsonSchemaAndUnknownComponentDoNotReplaceCandidate)
{
    WorldPtr original = World::CreateWorld();
    original->CreateActor("Root");
    WorldAssetData candidate;
    std::string    error;
    ASSERT_TRUE(CaptureWorldAsset(*original, candidate, error));
    const auto originalId = candidate.actors[0].uuid;
    for (const auto text : {"{}", "{\"actors\":3,\"assetRoots\":[],\"subSystems\":[]}",
                            "{\"actors\":[{\"uuid\":\"11111111-2222-4333-8444-555555555555\",\"name\":\"Unknown\",\"components\":[{\"type\":"
                            "\"AAAAAAAA-BBBB-4CCC-8DDD-EEEEEEEEEEEE\",\"data\":{}}]}],\"assetRoots\":[],\"subSystems\":[]}"}) {
        EXPECT_FALSE(Cook(text, candidate, error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(candidate.actors[0].uuid, originalId);
    }
    const std::string deep = std::string(100, '[') + "0" + std::string(100, ']');
    EXPECT_FALSE(Cook(deep, candidate, error));
    EXPECT_NE(error.find("budget"), std::string::npos);
}

TEST_F(WorldAssetTest, SubsystemConstructionFailurePreservesTheExistingWorld)
{
    WorldPtr original = World::CreateWorld();
    original->CreateActor("Root");
    original->GetMutableWorldDesc()->subSystems.push_back({Name("WorldBinaryTest"), Any(WorldBinaryConfig{}), true});
    WorldAssetData data;
    std::string    error;
    ASSERT_TRUE(CaptureWorldAsset(*original, data, error)) << error;
    auto registration    = *WorldSubSystemRegistry::Get().GetRegistration(Name("WorldBinaryTest"));
    registration.factory = [](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> { return {}; };
    WorldSubSystemRegistry::Get().Unregister(Name("WorldBinaryTest"));
    ASSERT_TRUE(WorldSubSystemRegistry::Get().Register(Name("WorldBinaryTest"), std::move(registration)));
    WorldPtr preserved = original;
    EXPECT_FALSE(InstantiateWorldAsset(data, preserved, error));
    EXPECT_EQ(preserved.Get(), original.Get());
    EXPECT_FALSE(error.empty());
}

TEST_F(WorldAssetTest, CyclesMissingParentsUnknownSubsystemAndUnsupportedSchemaFail)
{
    WorldPtr       original = World::CreateWorld();
    auto          *first    = original->CreateActor("First");
    auto          *second   = original->CreateActor("Second");
    WorldAssetData data;
    std::string    error;
    ASSERT_TRUE(CaptureWorldAsset(*original, data, error));
    auto *transform = first->GetComponent<TransformComponent>();
    transform->SetParentPreserveLocal(second->GetComponent<TransformComponent>());
    ASSERT_TRUE(CaptureWorldAsset(*original, data, error));
    auto changed = data;
    changed.actors.erase(changed.actors.begin() + 1);
    EXPECT_FALSE(changed.Validate(error));
    WorldPtr preserved = original;
    EXPECT_FALSE(InstantiateWorldAsset(changed, preserved, error));
    EXPECT_EQ(preserved.Get(), original.Get());
    auto       cyclic   = Source(*original);
    const auto empty    = Uuid{}.ToString();
    const auto position = cyclic.find(empty);
    ASSERT_NE(position, std::string::npos);
    cyclic.replace(position, empty.size(), first->GetUuid().ToString());
    EXPECT_FALSE(Cook(cyclic, changed, error));
    changed = data;
    changed.actors.push_back(changed.actors[0]);
    EXPECT_FALSE(changed.Validate(error));
    changed = data;
    changed.subSystems.push_back({"UnknownWorldSubsystem", false, {}, {}});
    EXPECT_FALSE(changed.Validate(error));
    WorldPtr unsupported = World::CreateWorld();
    unsupported->CreateActor("Unsupported", false)->AddComponent<WorldBinaryUnsupportedComponent>();
    EXPECT_FALSE(CaptureWorldAsset(*unsupported, changed, error));
    EXPECT_FALSE(error.empty());
}
