#include <softbody/editor/SoftBodyGenerator.h>

#include <aurora/editor/MeshGeneratorDocumentContract.h>
#include <core/async/ThreadPool.h>
#include <gtest/gtest.h>
#include <softbody/assets/SoftBodyRecipe.h>

using namespace sky;
using namespace sky::editor;
using namespace sky::aurora::editor;
using namespace sky::softbody;

namespace {
    class Input final : public IAssetGeneratorPayload, public IMeshGeneratorInput {
    public:
        const void *QueryInterface(const Uuid &id) const override
        {
            return id == MESH_GENERATOR_INPUT_INTERFACE ? static_cast<const IMeshGeneratorInput *>(this) : nullptr;
        }
        const MeshGeneratorInputData &GetMeshInput() const override
        {
            return value;
        }
        MeshGeneratorInputData value;
    };
} // namespace

TEST(SoftBodyGenerator, RegisteredProviderProducesFourWeightCandidateAndReopenableRecipe)
{
    const auto owner    = Uuid::Create();
    auto      *registry = AssetGeneratorRegistry::Get();
    ASSERT_TRUE(registry->Register(owner, CreateSoftBodyGenerator()));
    struct Cleanup {
        Uuid owner;
        ~Cleanup()
        {
            AssetGeneratorRegistry::Get()->UnregisterOwner(owner);
        }
    } cleanup{owner};
    const auto snapshot = registry->Find(RIG_GENERATOR_ID);
    Any        settings;
    ASSERT_TRUE(registry->CreateSettings(snapshot, settings));
    auto *config = settings.GetAs<RigAuthoringSettings>();
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->generator.maxControlPoints, 30u);
    EXPECT_TRUE(config->material.enableRootFollow);
    config->generator.maxVolumeExcess = 4.f;
    config->materialSlots             = {Uuid::Create()};
    const auto payload                = std::make_shared<Input>();
    auto      &source                 = payload->value;
    source.sourceOwner                = Uuid::Create();
    source.sourceKey = source.mesh.key = "body";
    phy::AuthoringMeshPrimitive primitive;
    primitive.key       = "body/primitive/0";
    primitive.positions = {{.2f, 0, 0}, {-.2f, 0, 0}, {0, .2f, 0}, {0, -.2f, 0}, {0, 0, .2f}, {0, 0, -.2f}};
    primitive.indices   = {0, 2, 4, 2, 1, 4, 1, 3, 4, 3, 0, 4, 2, 0, 5, 1, 2, 5, 3, 1, 5, 0, 3, 5};
    source.mesh.primitives.push_back(std::move(primitive));
    source.controls = {{"center", {0, 0, 0}, MeshControlMode::Free}};
    AssetGenerationInput input;
    input.context  = {source.sourceOwner, Uuid::Create(), "AuroraMesh", 1, {"mesh-binding"}};
    input.payload  = payload;
    input.settings = settings;
    ThreadPool                              pool{1};
    std::shared_ptr<AssetGenerationRequest> request;
    ASSERT_TRUE(registry->Start(snapshot, input, pool, request));
    request->Wait();
    AssetGenerationResult result;
    const auto            status = request->TakeResult(input.context, result);
    ASSERT_TRUE(status) << status.message;
    const auto *queried = result.payload->QueryInterface(MESH_GENERATOR_OUTPUT_INTERFACE);
    ASSERT_NE(queried, nullptr);
    const auto &generated = static_cast<const IMeshGeneratorOutput *>(queried)->GetMeshOutput();
    EXPECT_LE(generated.cagePoints.size(), 30u);
    EXPECT_EQ(generated.binding.bones.size(), generated.cagePoints.size() + 1);
    EXPECT_EQ(generated.binding.materialSlots, config->materialSlots);
    EXPECT_FALSE(generated.diagnostics.empty());
    EXPECT_EQ(generated.recipeExtension, "softbodyrig");
    MeshGeneratorRecipeState reopened;
    const auto              *begin = reinterpret_cast<const uint8_t *>(generated.recipeText.data());
    ASSERT_TRUE(CreateSoftBodyRecipeCodec()->Decode({begin, generated.recipeText.size()}, reopened));
    EXPECT_EQ(reopened.sourceOwner, source.sourceOwner);
    EXPECT_EQ(reopened.sourceKey, source.sourceKey);
    EXPECT_EQ(reopened.controls, source.controls);
    ASSERT_NE(reopened.settings.GetAsConst<RigAuthoringSettings>(), nullptr);
    EXPECT_EQ(reopened.settings.GetAsConst<RigAuthoringSettings>()->bodyPrimitive, "body/primitive/0");
    EXPECT_TRUE(reopened.settings.GetAsConst<RigAuthoringSettings>()->material.enableRootFollow);
    EXPECT_EQ(reopened.settings.GetAsConst<RigAuthoringSettings>()->materialSlots, config->materialSlots);
}

TEST(SoftBodyGenerator, MalformedRecipeDoesNotReplaceDocumentState)
{
    MeshGeneratorRecipeState state;
    state.sourceOwner       = Uuid::Create();
    state.generatorId       = "sentinel";
    const auto        owner = state.sourceOwner;
    const std::string text  = "{\"version\":1}";
    EXPECT_FALSE(CreateSoftBodyRecipeCodec()->Decode({reinterpret_cast<const uint8_t *>(text.data()), text.size()}, state));
    EXPECT_EQ(state.sourceOwner, owner);
    EXPECT_EQ(state.generatorId, "sentinel");
}
