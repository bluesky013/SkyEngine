#include "ShaderArtifactTestHelper.h"

#include <aurora/rhi/Shader.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {
    ShaderBlockMember FloatField(const std::string &name, uint32_t offset)
    {
        return {.name = name, .offset = offset, .size = 4, .scalarType = ShaderScalarType::FLOAT, .kind = ShaderTypeKind::SCALAR, .alignment = 4};
    }

    ShaderBlockMember Float4Field(const std::string &name, uint32_t offset)
    {
        return {.name       = name,
                .offset     = offset,
                .size       = 16,
                .scalarType = ShaderScalarType::FLOAT,
                .kind       = ShaderTypeKind::VECTOR,
                .cols       = 4,
                .alignment  = 16};
    }

    ShaderReflection MaterialReflection()
    {
        ShaderResource material;
        material.name               = "Material";
        material.type               = ShaderResourceType::UNIFORM_BUFFER;
        material.set                = 2;
        material.binding            = 3;
        material.stages             = ShaderStageFlagBit::CS;
        material.declarationOwner   = "rhi-test/parameter-transaction";
        auto image                  = material;
        image.name                  = "Images";
        image.type                  = ShaderResourceType::SAMPLED_IMAGE;
        image.binding               = 5;
        image.count                 = 2;
        image.descriptorArray       = true;
        image.texture               = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4};
        auto sampler                = material;
        sampler.name                = "Filter";
        sampler.type                = ShaderResourceType::SAMPLER;
        sampler.binding             = 7;
        sampler.sampler             = ShaderSamplerType::REGULAR;
        auto optional               = material;
        optional.name               = "Optional";
        optional.binding            = 9;
        optional.stages             = {};
        auto unusedImage            = image;
        unusedImage.name            = "UnusedImage";
        unusedImage.binding         = 11;
        unusedImage.count           = 1;
        unusedImage.descriptorArray = false;
        unusedImage.stages          = {};
        auto unusedSampler          = sampler;
        unusedSampler.name          = "UnusedSampler";
        unusedSampler.binding       = 13;
        unusedSampler.stages        = {};
        ShaderReflection reflection;
        reflection.resources      = {material, image, sampler};
        reflection.declarations   = {material, image, sampler, optional, unusedImage, unusedSampler};
        reflection.blocks         = {{"Material", "MaterialData", 2, 3, 32, {Float4Field("tint", 0), FloatField("gain", 16)}, 16}};
        reflection.declaredBlocks = reflection.blocks;
        reflection.declaredBlocks.push_back({"Optional", "OptionalData", 2, 9, 4, {FloatField("value", 0)}, 4});
        reflection.nativeBindings = {{2, 3, ShaderStageFlagBit::CS, 3, ShaderNativeCategory::DESCRIPTOR},
                                     {2, 5, ShaderStageFlagBit::CS, 5, ShaderNativeCategory::DESCRIPTOR, 0, 2},
                                     {2, 7, ShaderStageFlagBit::CS, 7, ShaderNativeCategory::DESCRIPTOR}};
        return reflection;
    }

    class CpuFunction final : public ShaderFunction {
    public:
        bool Init(ShaderStageArtifactPtr artifact, std::string &error)
        {
            Descriptor descriptor{};
            descriptor.stage    = ShaderStageFlagBit::CS;
            descriptor.artifact = std::move(artifact);
            return InitArtifact(descriptor, API::VULKAN, error);
        }
    };

    class CpuShader final : public Shader {
    public:
        bool Init(ShaderReflection input, std::span<const ShaderParameterAlias> aliases, std::string &error)
        {
            const std::array<uint8_t, 20> spirv{3, 2, 35, 7, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
            auto artifact = MakeTestShaderArtifact(ShaderTarget::SPIRV, ShaderStageFlagBit::CS, spirv, "main", std::move(input));
            if (!artifact || !function.Init(artifact, error))
                return false;
            reflection = artifact->reflection;
            Descriptor descriptor{};
            descriptor.cs = &function;
            descriptor.parameterAliases.assign(aliases.begin(), aliases.end());
            return InitParameterTable(descriptor, error);
        }
        const ShaderReflection &GetReflection() const override
        {
            return reflection;
        }
        size_t CachedPlanCount() const
        {
            std::lock_guard lock(uniformPlanMutex);
            return uniformPlans.size();
        }

    private:
        CpuFunction      function;
        ShaderReflection reflection;
    };

    class ShaderParameterTransactionTest : public testing::Test {
    protected:
        void SetUp() override
        {
            const ShaderParameterAlias aliases[] = {{"tintAlias", "Material.tint"}, {"imageAlias", "Images"}};
            std::string                error;
            ASSERT_TRUE(shader.Init(MaterialReflection(), aliases, error)) << error;
        }
        void Required(ShaderParameterTransaction &transaction)
        {
            EXPECT_TRUE(transaction.WriteData(Name("Material.tint"), MakeShaderData(Vector4(.25f, .5f, .75f, 1.f))));
            EXPECT_TRUE(transaction.WriteData(Name("Material.gain"), MakeShaderData(2.f)));
            EXPECT_TRUE(transaction.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 101, 0));
            EXPECT_TRUE(transaction.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 102, 1));
            EXPECT_TRUE(transaction.WriteResource(Name("Filter"), ShaderResourceType::SAMPLER, 201));
        }
        CpuShader shader;
    };

    float ReadFloat(const std::vector<std::byte> &bytes, uint32_t offset)
    {
        float value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
} // namespace

TEST_F(ShaderParameterTransactionTest, NamesAliasesAndHandlesPublishOneImmutablePacket)
{
    ShaderParameterHandle gain;
    ASSERT_EQ(shader.GetParameterTable().Resolve(Name("Material.gain"), 2, gain), ShaderParameterLookup::READY);
    ShaderParameterTransaction transaction(shader, 2);
    EXPECT_EQ(transaction.WriteData(Name("tintAlias"), MakeShaderData(Vector4(.25f, .5f, .75f, 1.f))).status, ShaderParameterWriteStatus::SUCCESS);
    EXPECT_TRUE(transaction.WriteData(gain, MakeShaderData(2.f)));
    EXPECT_TRUE(transaction.WriteResource(Name("imageAlias"), ShaderResourceType::SAMPLED_IMAGE, 101));
    EXPECT_TRUE(transaction.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 102, 1));
    EXPECT_TRUE(transaction.WriteResource(Name("Filter"), ShaderResourceType::SAMPLER, 201));
    ASSERT_TRUE(transaction.Complete()) << transaction.GetError();
    std::shared_ptr<const ShaderParameterPacket> packet;
    ASSERT_TRUE(transaction.Finalize(packet)) << transaction.GetError();
    ASSERT_TRUE(packet);
    EXPECT_EQ(packet->generation, shader.GetParameterTable().generation);
    EXPECT_EQ(packet->set, 2u);
    ASSERT_EQ(packet->uniforms.size(), 1u);
    EXPECT_EQ(packet->uniforms[0].binding, 3u);
    EXPECT_EQ(packet->uniforms[0].type, ShaderResourceType::UNIFORM_BUFFER);
    EXPECT_GE(packet->uniforms[0].alignment, 16u);
    EXPECT_EQ(packet->uniforms[0].alignment & (packet->uniforms[0].alignment - 1), 0u);
    ASSERT_EQ(packet->uniforms[0].bytes.size(), 32u);
    EXPECT_FLOAT_EQ(ReadFloat(packet->uniforms[0].bytes, 0), .25f);
    EXPECT_FLOAT_EQ(ReadFloat(packet->uniforms[0].bytes, 16), 2.f);
    ASSERT_EQ(packet->resources.size(), 3u);
    const auto secondImage = std::ranges::find_if(packet->resources, [](const auto &write) { return write.binding == 5 && write.element == 1; });
    ASSERT_NE(secondImage, packet->resources.end());
    EXPECT_EQ(secondImage->token, 102u);
    EXPECT_EQ(secondImage->type, ShaderResourceType::SAMPLED_IMAGE);
    EXPECT_EQ(shader.CachedPlanCount(), 1u);
    const auto published = packet;
    EXPECT_FALSE(transaction.WriteData(Name("Material.gain"), MakeShaderData(3.f)));
    EXPECT_FALSE(transaction.Finalize(packet));
    EXPECT_EQ(packet, published);
}

TEST_F(ShaderParameterTransactionTest, InactiveWritesValidateTypesWithoutEnteringThePacket)
{
    ShaderParameterTransaction transaction(shader, 2);
    EXPECT_EQ(transaction.WriteData(Name("Optional.value"), MakeShaderData(1.f)).status, ShaderParameterWriteStatus::INACTIVE);
    EXPECT_EQ(transaction.WriteResource(Name("UnusedImage"), ShaderResourceType::SAMPLED_IMAGE, 301).status, ShaderParameterWriteStatus::INACTIVE);
    EXPECT_EQ(transaction.WriteResource(Name("UnusedSampler"), ShaderResourceType::SAMPLER, 302).status, ShaderParameterWriteStatus::INACTIVE);
    EXPECT_TRUE(transaction.GetState().fields.empty());
    EXPECT_TRUE(transaction.GetState().resources.empty());
    Required(transaction);
    std::shared_ptr<const ShaderParameterPacket> packet;
    ASSERT_TRUE(transaction.Finalize(packet)) << transaction.GetError();
    ASSERT_EQ(packet->uniforms.size(), 1u);
    EXPECT_EQ(packet->resources.size(), 3u);

    ShaderParameterTransaction wrongData(shader, 2);
    EXPECT_EQ(wrongData.WriteData(Name("Optional.value"), MakeShaderData(uint32_t{1})).status, ShaderParameterWriteStatus::FAILED);
    EXPECT_FALSE(wrongData.Complete());
    const auto published = packet;
    EXPECT_FALSE(wrongData.Finalize(packet));
    EXPECT_EQ(packet, published);
    ShaderParameterTransaction wrongResource(shader, 2);
    EXPECT_EQ(wrongResource.WriteResource(Name("UnusedImage"), ShaderResourceType::SAMPLER, 301).status, ShaderParameterWriteStatus::FAILED);
    EXPECT_FALSE(wrongResource.Finalize(packet));
    EXPECT_EQ(packet, published);
    ShaderParameterTransaction wrongElement(shader, 2);
    EXPECT_EQ(wrongElement.WriteResource(Name("UnusedImage"), ShaderResourceType::SAMPLED_IMAGE, 301, 1).status, ShaderParameterWriteStatus::FAILED);
}

TEST_F(ShaderParameterTransactionTest, PartialInheritedStateRetainsUnchangedValuesAndPreviousPacket)
{
    ShaderParameterTransaction initial(shader, 2);
    Required(initial);
    std::shared_ptr<const ShaderParameterPacket> previous;
    ASSERT_TRUE(initial.Finalize(previous)) << initial.GetError();
    auto inherited = initial.GetState();
    inherited.fields.at(3).erase("gain");
    inherited.resources.erase({5, 1});
    ShaderParameterTransaction update(shader, 2, inherited);
    EXPECT_TRUE(update.WriteData(Name("Material.gain"), MakeShaderData(3.f)));
    EXPECT_TRUE(update.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 202, 1));
    std::shared_ptr<const ShaderParameterPacket> current;
    ASSERT_TRUE(update.Finalize(current)) << update.GetError();
    ASSERT_EQ(previous->uniforms.size(), 1u);
    ASSERT_EQ(current->uniforms.size(), 1u);
    EXPECT_FLOAT_EQ(ReadFloat(previous->uniforms[0].bytes, 16), 2.f);
    EXPECT_FLOAT_EQ(ReadFloat(current->uniforms[0].bytes, 16), 3.f);
    EXPECT_EQ(std::memcmp(previous->uniforms[0].bytes.data(), current->uniforms[0].bytes.data(), 16), 0);
    EXPECT_EQ(initial.GetState().resources.at({5, 1}).token, 102u);
    EXPECT_EQ(update.GetState().resources.at({5, 1}).token, 202u);
    EXPECT_EQ(shader.CachedPlanCount(), 1u);

    ShaderParameterTransaction partialOverride(shader, 2, initial.GetState());
    EXPECT_TRUE(partialOverride.WriteData(Name("Material.gain"), MakeShaderData(4.f)));
    EXPECT_TRUE(partialOverride.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 203, 1));
    ASSERT_TRUE(partialOverride.Finalize(current)) << partialOverride.GetError();
    EXPECT_FLOAT_EQ(ReadFloat(current->uniforms[0].bytes, 16), 4.f);
    EXPECT_FLOAT_EQ(ReadFloat(previous->uniforms[0].bytes, 16), 2.f);
}

TEST_F(ShaderParameterTransactionTest, MissingRequiredFieldsAndArrayElementsCannotPublish)
{
    auto sentinel = std::make_shared<const ShaderParameterPacket>();
    for (uint32_t omitted = 0; omitted < 5; ++omitted) {
        SCOPED_TRACE(omitted);
        ShaderParameterTransaction transaction(shader, 2);
        if (omitted != 0)
            EXPECT_TRUE(transaction.WriteData(Name("Material.tint"), MakeShaderData(Vector4(1.f))));
        if (omitted != 1)
            EXPECT_TRUE(transaction.WriteData(Name("Material.gain"), MakeShaderData(2.f)));
        if (omitted != 2)
            EXPECT_TRUE(transaction.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 101));
        if (omitted != 3)
            EXPECT_TRUE(transaction.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 102, 1));
        if (omitted != 4)
            EXPECT_TRUE(transaction.WriteResource(Name("Filter"), ShaderResourceType::SAMPLER, 201));
        auto output = sentinel;
        EXPECT_FALSE(transaction.Complete());
        EXPECT_FALSE(transaction.Finalize(output));
        EXPECT_EQ(output, sentinel);
    }
    EXPECT_EQ(shader.CachedPlanCount(), 0u);
}

TEST_F(ShaderParameterTransactionTest, UnknownStaleAndDuplicateWritesKeepTheFirstErrorSticky)
{
    ShaderParameterTransaction unknown(shader, 2);
    EXPECT_FALSE(unknown.WriteData(Name("Missing"), MakeShaderData(1.f)));
    const auto error = unknown.GetError();
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(unknown.WriteData(Name("Material.gain"), MakeShaderData(2.f)));
    EXPECT_FALSE(unknown.Fail("second failure"));
    EXPECT_EQ(unknown.GetError(), error);
    ShaderParameterHandle stale;
    ASSERT_EQ(shader.GetParameterTable().Resolve(Name("Material.gain"), 2, stale), ShaderParameterLookup::READY);
    ++stale.generation;
    ShaderParameterTransaction staleUpdate(shader, 2);
    EXPECT_FALSE(staleUpdate.WriteData(stale, MakeShaderData(2.f)));
    EXPECT_FALSE(staleUpdate.Complete());
    ShaderParameterTransaction duplicate(shader, 2);
    EXPECT_TRUE(duplicate.WriteData(Name("Material.tint"), MakeShaderData(Vector4(1.f))));
    EXPECT_FALSE(duplicate.WriteData(Name("tintAlias"), MakeShaderData(Vector4(.5f))));
    ShaderParameterTransaction duplicateResource(shader, 2);
    EXPECT_TRUE(duplicateResource.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 101));
    EXPECT_FALSE(duplicateResource.WriteResource(Name("imageAlias"), ShaderResourceType::SAMPLED_IMAGE, 102));
    ShaderParameterTransaction wrongType(shader, 2);
    EXPECT_FALSE(wrongType.WriteData(Name("Material.gain"), MakeShaderData(uint32_t{2})));
    ShaderParameterTransaction wrongElement(shader, 2);
    EXPECT_FALSE(wrongElement.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 103, 2));
    std::shared_ptr<const ShaderParameterPacket> output;
    EXPECT_FALSE(duplicate.Finalize(output));
    EXPECT_FALSE(duplicateResource.Finalize(output));
    EXPECT_FALSE(wrongType.Finalize(output));
    EXPECT_FALSE(output);
}

TEST_F(ShaderParameterTransactionTest, WholeBlocksAndOpaqueUniformResourcesCannotOverlapFieldWrites)
{
    const auto                     &block = shader.GetReflection().blocks.front();
    const ShaderCpuSchema           schema{block.size, block.alignment, block.members};
    const std::array<std::byte, 32> bytes{};
    ShaderParameterTransaction      blockFirst(shader, 2);
    EXPECT_TRUE(blockFirst.WriteBlock(Name("Material"), schema, bytes));
    EXPECT_FALSE(blockFirst.WriteData(Name("Material.tint"), MakeShaderData(Vector4(1.f))));
    ShaderParameterTransaction fieldFirst(shader, 2);
    EXPECT_TRUE(fieldFirst.WriteData(Name("Material.gain"), MakeShaderData(2.f)));
    EXPECT_FALSE(fieldFirst.WriteBlock(Name("Material"), schema, bytes));
    ShaderParameterTransaction resourceFirst(shader, 2);
    EXPECT_TRUE(resourceFirst.WriteResource(Name("Material"), ShaderResourceType::UNIFORM_BUFFER, 401));
    EXPECT_FALSE(resourceFirst.WriteData(Name("Material.gain"), MakeShaderData(2.f)));
    ShaderParameterTransaction fieldBeforeResource(shader, 2);
    EXPECT_TRUE(fieldBeforeResource.WriteData(Name("Material.gain"), MakeShaderData(2.f)));
    EXPECT_FALSE(fieldBeforeResource.WriteResource(Name("Material"), ShaderResourceType::UNIFORM_BUFFER, 401));
    ShaderParameterTransaction blockBeforeResource(shader, 2);
    EXPECT_TRUE(blockBeforeResource.WriteBlock(Name("Material"), schema, bytes));
    EXPECT_FALSE(blockBeforeResource.WriteResource(Name("Material"), ShaderResourceType::UNIFORM_BUFFER, 401));

    ShaderParameterTransaction opaque(shader, 2);
    EXPECT_TRUE(opaque.WriteResource(Name("Material"), ShaderResourceType::UNIFORM_BUFFER, 401));
    EXPECT_TRUE(opaque.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 101));
    EXPECT_TRUE(opaque.WriteResource(Name("Images"), ShaderResourceType::SAMPLED_IMAGE, 102, 1));
    EXPECT_TRUE(opaque.WriteResource(Name("Filter"), ShaderResourceType::SAMPLER, 201));
    std::shared_ptr<const ShaderParameterPacket> packet;
    ASSERT_TRUE(opaque.Finalize(packet)) << opaque.GetError();
    EXPECT_TRUE(packet->uniforms.empty());
    EXPECT_EQ(packet->resources.size(), 4u);
}

TEST_F(ShaderParameterTransactionTest, CancelledTransactionCannotReplaceAPublishedPacket)
{
    ShaderParameterTransaction transaction(shader, 2);
    Required(transaction);
    transaction.Cancel();
    auto       output   = std::make_shared<const ShaderParameterPacket>();
    const auto previous = output;
    EXPECT_FALSE(transaction.Complete());
    EXPECT_FALSE(transaction.Finalize(output));
    EXPECT_EQ(output, previous);
    EXPECT_FALSE(transaction.WriteData(Name("Material.gain"), MakeShaderData(4.f)));
    EXPECT_EQ(shader.CachedPlanCount(), 0u);
}

TEST_F(ShaderParameterTransactionTest, BorrowedStateValidationAcceptsTypedAndOpaqueValuesWithoutPacking)
{
    ShaderParameterTransaction transaction(shader, 2);
    Required(transaction);
    const auto &state  = transaction.GetState();
    const auto  fields = state.fields;
    std::string error;
    ASSERT_TRUE(ValidateShaderParameterState(shader, 2, state, error)) << error;
    EXPECT_EQ(state.fields, fields);
    EXPECT_EQ(state.resources.at({5, 1}).token, 102u);
    EXPECT_EQ(shader.CachedPlanCount(), 0u);
    EXPECT_TRUE(transaction.Complete()) << transaction.GetError();
    EXPECT_EQ(shader.CachedPlanCount(), 0u);

    auto opaque = state;
    opaque.fields.clear();
    opaque.resources[{3, 0}] = {3, 0, 401, ShaderResourceType::UNIFORM_BUFFER_DYNAMIC};
    ASSERT_TRUE(ValidateShaderParameterState(shader, 2, opaque, error)) << error;
    ShaderParameterTransaction                   inherited(shader, 2, opaque);
    std::shared_ptr<const ShaderParameterPacket> packet;
    ASSERT_TRUE(inherited.Finalize(packet)) << inherited.GetError();
    EXPECT_TRUE(packet->uniforms.empty());
    ASSERT_EQ(packet->resources.size(), 4u);
    EXPECT_EQ(packet->resources.front().binding, 3u);
    EXPECT_EQ(packet->resources.front().type, ShaderResourceType::UNIFORM_BUFFER);
    EXPECT_EQ(shader.CachedPlanCount(), 0u);
}

TEST_F(ShaderParameterTransactionTest, BorrowedStateRejectsUnusedAndMalformedProvidersBeforePublishing)
{
    ShaderParameterTransaction source(shader, 2);
    Required(source);
    for (uint32_t fault = 0; fault < 16; ++fault) {
        SCOPED_TRACE(fault);
        auto state = source.GetState();
        switch (fault) {
        case 0: state.fields[99]["unused"] = MakeShaderData(1.f); break;
        case 1: state.fields[9]["value"] = MakeShaderData(1.f); break;
        case 2: state.fields[3]["unused"] = MakeShaderData(1.f); break;
        case 3: state.fields[5]["unused"] = MakeShaderData(1.f); break;
        case 4: state.resources[{99, 0}] = {99, 0, 301, ShaderResourceType::SAMPLED_IMAGE}; break;
        case 5: state.resources[{11, 0}] = {11, 0, 301, ShaderResourceType::SAMPLED_IMAGE}; break;
        case 6: state.resources[{5, 2}] = {5, 2, 301, ShaderResourceType::SAMPLED_IMAGE}; break;
        case 7: state.resources.at({5, 0}).binding = 7; break;
        case 8: state.resources.at({5, 1}).element = 0; break;
        case 9: state.resources.at({5, 0}).type = ShaderResourceType::SAMPLER; break;
        case 10: state.resources.at({5, 0}).token = UINT32_MAX; break;
        case 11: state.fields[3]["tint"].bytes.resize(1); break;
        case 12: state.fields[3]["gain"] = MakeShaderData(uint32_t{1}); break;
        case 13: state.resources[{3, 0}] = {3, 0, 401, ShaderResourceType::UNIFORM_BUFFER}; break;
        case 14: state.fields[3].clear(); break;
        case 15: state.resources.erase({5, 1}); break;
        }
        const auto  fields = state.fields;
        std::string error;
        EXPECT_FALSE(ValidateShaderParameterState(shader, 2, state, error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(state.fields, fields);
        ShaderParameterTransaction inherited(shader, 2, state);
        const auto                 sentinel = std::make_shared<const ShaderParameterPacket>();
        auto                       packet   = sentinel;
        EXPECT_FALSE(inherited.Complete());
        EXPECT_EQ(inherited.GetError(), error);
        EXPECT_FALSE(inherited.Finalize(packet));
        EXPECT_EQ(packet, sentinel);
    }
    EXPECT_EQ(shader.CachedPlanCount(), 0u);
}

TEST_F(ShaderParameterTransactionTest, BorrowedStateRequiresItsInitializedShaderAndActiveSet)
{
    ShaderParameterTransaction transaction(shader, 2);
    Required(transaction);
    std::string error;
    CpuShader   uninitialized;
    EXPECT_FALSE(ValidateShaderParameterState(uninitialized, 2, transaction.GetState(), error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(ValidateShaderParameterState(shader, 1, transaction.GetState(), error));
    EXPECT_TRUE(ValidateShaderParameterState(shader, 1, {}, error));
    EXPECT_EQ(shader.CachedPlanCount(), 0u);
}

TEST(ShaderParameterTransaction, TypedBlockExtractionPacksMatrixAndArrayExactlyOnceForTheTarget)
{
    auto matrix         = Float4Field("transform", 0);
    matrix.kind         = ShaderTypeKind::MATRIX;
    matrix.rows         = 4;
    matrix.size         = 64;
    matrix.matrixStride = 16;
    auto weights        = Float4Field("weights", 64);
    weights.kind        = ShaderTypeKind::ARRAY;
    weights.arrayCount  = 2;
    weights.arrayStride = 16;
    weights.size        = 32;
    const ShaderCpuSchema cpu{112, 16, {matrix, weights, FloatField("gain", 96)}};
    auto                  nativeMatrix = matrix;
    nativeMatrix.offset                = 16;
    nativeMatrix.matrixLayout          = ShaderMatrixLayout::ROW_MAJOR;
    auto nativeWeights                 = weights;
    nativeWeights.offset               = 96;
    nativeWeights.arrayStride          = 32;
    nativeWeights.size                 = 64;
    ShaderBlockLayout target{"Packed", "PackedData", 2, 3, 176, {FloatField("gain", 0), nativeMatrix, nativeWeights}, 16};
    ShaderReflection  reflection;
    ShaderResource    resource;
    resource.name             = target.name;
    resource.type             = ShaderResourceType::UNIFORM_BUFFER;
    resource.set              = 2;
    resource.binding          = 3;
    resource.stages           = ShaderStageFlagBit::CS;
    resource.declarationOwner = "rhi-test/parameter-transaction-packed";
    reflection.resources = reflection.declarations = {resource};
    reflection.blocks = reflection.declaredBlocks = {target};
    reflection.nativeBindings                     = {{2, 3, ShaderStageFlagBit::CS, 3, ShaderNativeCategory::DESCRIPTOR}};
    CpuShader   shader;
    std::string error;
    ASSERT_TRUE(shader.Init(std::move(reflection), {}, error)) << error;
    std::vector<std::byte>      source(cpu.size, std::byte{0xcd});
    const std::array<float, 16> transform{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const Vector4               values[] = {{1, 2, 3, 4}, {5, 6, 7, 8}};
    const float                 gain     = 2.5f;
    std::memcpy(source.data(), transform.data(), sizeof(transform));
    std::memcpy(source.data() + 64, values, sizeof(values));
    std::memcpy(source.data() + 96, &gain, sizeof(gain));
    ShaderParameterTransaction transaction(shader, 2);
    ASSERT_TRUE(transaction.WriteBlock(Name("Packed"), cpu, source)) << transaction.GetError();
    const auto &fields = transaction.GetState().fields.at(3);
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields.at("transform").matrixLayout, ShaderMatrixLayout::COLUMN_MAJOR);
    EXPECT_EQ(fields.at("weights").count, 2u);
    const std::vector<std::pair<std::string, ShaderDataValue>> logical(fields.begin(), fields.end());
    std::vector<std::byte>                                     expected;
    ASSERT_TRUE(PackShaderData(target, logical, expected, error)) << error;
    std::shared_ptr<const ShaderParameterPacket> packet;
    ASSERT_TRUE(transaction.Finalize(packet)) << transaction.GetError();
    ASSERT_EQ(packet->uniforms.size(), 1u);
    EXPECT_EQ(packet->uniforms[0].bytes, expected);
    const auto cachedPlans = shader.CachedPlanCount();
    EXPECT_GT(cachedPlans, 0u);
    EXPECT_FLOAT_EQ(ReadFloat(packet->uniforms[0].bytes, 0), gain);
    for (uint32_t row = 0; row < 4; ++row)
        for (uint32_t column = 0; column < 4; ++column)
            EXPECT_FLOAT_EQ(ReadFloat(packet->uniforms[0].bytes, 16 + row * 16 + column * 4), transform[column * 4 + row]);
    EXPECT_FLOAT_EQ(ReadFloat(packet->uniforms[0].bytes, 96), 1.f);
    EXPECT_FLOAT_EQ(ReadFloat(packet->uniforms[0].bytes, 128), 5.f);
    for (uint32_t offset = 112; offset < 128; ++offset)
        EXPECT_EQ(packet->uniforms[0].bytes[offset], std::byte{});
    ShaderParameterTransaction repeated(shader, 2);
    ASSERT_TRUE(repeated.WriteBlock(Name("Packed"), cpu, source)) << repeated.GetError();
    ASSERT_TRUE(repeated.Finalize(packet)) << repeated.GetError();
    EXPECT_EQ(packet->uniforms[0].bytes, expected);
    EXPECT_EQ(shader.CachedPlanCount(), cachedPlans);
    ShaderParameterTransaction wrongCount(shader, 2);
    EXPECT_FALSE(wrongCount.WriteData(Name("Packed.weights"), MakeShaderData(std::span<const Vector4>(values, 1))));
}
