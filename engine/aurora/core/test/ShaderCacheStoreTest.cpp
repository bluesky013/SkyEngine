#include <aurora/shader/ShaderCacheStore.h>

#include "ShaderArtifactTest.h"

#include <algorithm>
#include <core/file/MultiFileSystem.h>
#include <gtest/gtest.h>
#include <limits>
#include <type_traits>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST(ShaderCacheStoreTest, CompleteArtifactAndIndexRoundTrip)
{
    const auto        key      = TestCacheKey();
    auto              artifact = TestArtifact(key);
    ShaderBlockLayout block;
    block.name       = "parameters";
    block.structName = "Parameters";
    block.size       = 192;
    block.alignment  = 16;
    ShaderBlockMember matrix{"transform", 0, 64, ShaderScalarType::FLOAT, ShaderTypeKind::MATRIX, 4, 4};
    matrix.alignment    = 16;
    matrix.matrixStride = 16;
    matrix.matrixLayout = ShaderMatrixLayout::ROW_MAJOR;
    ShaderBlockMember array{"lights", 64, 128, ShaderScalarType::FLOAT, ShaderTypeKind::ARRAY, 1, 4, 8, 16};
    array.alignment = 16;
    block.members   = {matrix, array};
    ShaderResource resource{"parameters", ShaderResourceType::UNIFORM_BUFFER, 0, 0, 1};
    resource.stages                                = ShaderStageFlags(ShaderStageFlagBit::FS);
    resource.declarationOwner                      = "engine/test.slangh";
    artifact.reflection.resources                  = {resource};
    artifact.reflection.declarations               = {resource};
    artifact.reflection.blocks                     = {block};
    artifact.reflection.declaredBlocks             = {block};
    artifact.reflection.nativeBindings             = {{0, 0, ShaderStageFlagBit::FS, 2}};
    artifact.reflection.nativeBindings[0].category = ShaderNativeCategory::DESCRIPTOR;
    std::string error;
    ASSERT_TRUE(SealShaderArtifact(artifact, error)) << error;
    ShaderCacheBlob decoded;
    ASSERT_TRUE(DecodeBlob(EncodeBlob(artifact), decoded));
    EXPECT_EQ(decoded.identity, artifact.identity);
    EXPECT_EQ(decoded.artifactId, artifact.artifactId);
    EXPECT_EQ(decoded.data, artifact.data);
    ASSERT_EQ(decoded.reflection.blocks.size(), 1u);
    EXPECT_EQ(decoded.reflection.blocks[0].members, block.members);
    EXPECT_EQ(decoded.reflection.blocks[0].alignment, 16u);
    EXPECT_EQ(decoded.reflection.nativeBindings, artifact.reflection.nativeBindings);
    EXPECT_EQ(decoded.reflection.declarations[0].declarationOwner, resource.declarationOwner);
    ShaderCacheIndex index;
    ASSERT_TRUE(UpsertArtifact(index, key.sourcePath, {}, key.schemaFp, key, artifact, {"shared.slangh"}, &error)) << error;
    ShaderCacheIndex restored;
    ASSERT_TRUE(DecodeIndex(EncodeIndex(index), restored));
    const auto *record = FindArtifact(*FindPathEntry(restored, key.sourcePath), key);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->artifactId, artifact.artifactId);
    EXPECT_EQ(record->identity, artifact.identity);
    EXPECT_EQ(record->sourceDeps, std::vector<std::string>{"shared.slangh"});
}

TEST(ShaderCacheStoreTest, PlatformEndIsExclusiveAndPreservesLinuxAndHarmonyOS)
{
    EXPECT_EQ(static_cast<uint32_t>(ShaderPlatform::LINUX), 5u);
    EXPECT_EQ(static_cast<uint32_t>(ShaderPlatform::HARMONYOS), 6u);
    for (const auto platform : {ShaderPlatform::LINUX, ShaderPlatform::HARMONYOS}) {
        const auto      key      = TestCacheKey({}, "s.slang", ShaderTarget::SPIRV, platform);
        const auto      artifact = TestArtifact(key);
        ShaderCacheBlob decoded;
        ASSERT_TRUE(DecodeBlob(EncodeBlob(artifact), decoded));
        EXPECT_EQ(decoded.identity.environment.platform, platform);
    }
    const auto valid = TestArtifact(TestCacheKey());
    for (const auto platform : {ShaderPlatform::END, static_cast<ShaderPlatform>(static_cast<uint32_t>(ShaderPlatform::END) + 1),
                                static_cast<ShaderPlatform>(UINT32_MAX)}) {
        auto invalid                          = valid;
        invalid.identity.environment.platform = platform;
        auto        environment               = invalid.identity.environment;
        std::string error;
        EXPECT_FALSE(NormalizeShaderEnvironment(environment, error));
        ShaderCacheBlob output = valid;
        EXPECT_FALSE(DecodeBlob(EncodeBlob(invalid), output));
        EXPECT_EQ(output.identity, valid.identity);
        EXPECT_EQ(output.artifactId, valid.artifactId);
    }
}

TEST(ShaderCacheStoreTest, AllSerializedEnumBoundsRejectSentinelsAndOverflow)
{
    auto           valid = TestArtifact(TestCacheKey());
    ShaderResource resource{"parameters", ShaderResourceType::UNIFORM_BUFFER, 0, 0, 1};
    resource.stages           = valid.reflection.stages;
    resource.declarationOwner = "engine/test.slangh";
    ShaderBlockLayout block;
    block.name                                  = "parameters";
    block.structName                            = "Parameters";
    block.size                                  = 4;
    block.members                               = {{"value", 0, 4, ShaderScalarType::FLOAT, ShaderTypeKind::SCALAR}};
    valid.reflection.resources                  = {resource};
    valid.reflection.declarations               = {resource};
    valid.reflection.blocks                     = {block};
    valid.reflection.declaredBlocks             = {block};
    valid.reflection.nativeBindings             = {{0, 0, ShaderStageFlagBit::FS, 0}};
    valid.reflection.nativeBindings[0].category = ShaderNativeCategory::DESCRIPTOR;
    valid.identity.variants                     = {{"OPTION", 1, ShaderVariantType::BOOL}};
    valid.variantBindings                       = {{"OPTION", "", ShaderVariantType::BOOL, ShaderVariantLowering::COMPILE, 0, 1}};
    std::string error;
    ASSERT_TRUE(ValidateShaderReflection(valid.reflection, error)) << error;
    ASSERT_TRUE(SealShaderArtifact(valid, error)) << error;
    ShaderCacheBlob decoded;
    ASSERT_TRUE(DecodeBlob(EncodeBlob(valid), decoded));

    const auto rejectBoundary = [&](const char *name, const auto &field) {
        SCOPED_TRACE(name);
        auto invalid = valid;
        using Enum   = std::remove_cvref_t<decltype(field(invalid))>;
        for (const auto number : {static_cast<uint32_t>(Enum::END), static_cast<uint32_t>(Enum::END) + 1,
                                  static_cast<uint32_t>(std::numeric_limits<std::underlying_type_t<Enum>>::max())}) {
            SCOPED_TRACE(number);
            field(invalid) = static_cast<Enum>(number);
            // Keep fingerprints consistent so rejection exercises enum validation, not integrity checks.
            invalid.identity.interfaceFp = ShaderInterfaceFingerprint(invalid.reflection);
            invalid.artifactId           = ComputeShaderArtifactId(invalid);
            ShaderCacheBlob output       = valid;
            EXPECT_FALSE(DecodeBlob(EncodeBlob(invalid), output));
            EXPECT_EQ(output.identity, valid.identity);
            EXPECT_EQ(output.artifactId, valid.artifactId);
        }
    };
    rejectBoundary("identity.environment.target", [](auto &artifact) -> auto & { return artifact.identity.environment.target; });
    rejectBoundary("identity.environment.platform", [](auto &artifact) -> auto & { return artifact.identity.environment.platform; });
    rejectBoundary("identity.environment.codeFormat", [](auto &artifact) -> auto & { return artifact.identity.environment.codeFormat; });
    rejectBoundary("identity.environment.matrixLayout", [](auto &artifact) -> auto & { return artifact.identity.environment.matrixLayout; });
    rejectBoundary("identity.variants[0].type", [](auto &artifact) -> auto & { return artifact.identity.variants[0].type; });
    rejectBoundary("variantBindings[0].type", [](auto &artifact) -> auto & { return artifact.variantBindings[0].type; });
    rejectBoundary("variantBindings[0].lowering", [](auto &artifact) -> auto & { return artifact.variantBindings[0].lowering; });
    rejectBoundary("reflection.kind", [](auto &artifact) -> auto & { return artifact.reflection.kind; });
    rejectBoundary("reflection.bindingModel", [](auto &artifact) -> auto & { return artifact.reflection.bindingModel; });
    rejectBoundary("reflection.usage", [](auto &artifact) -> auto & { return artifact.reflection.usage; });
    rejectBoundary("reflection.resources[0].type", [](auto &artifact) -> auto & { return artifact.reflection.resources[0].type; });
    rejectBoundary("reflection.resources[0].texture.dimension",
                   [](auto &artifact) -> auto & { return artifact.reflection.resources[0].texture.dimension; });
    rejectBoundary("reflection.resources[0].texture.scalarType",
                   [](auto &artifact) -> auto & { return artifact.reflection.resources[0].texture.scalarType; });
    rejectBoundary("reflection.resources[0].sampler", [](auto &artifact) -> auto & { return artifact.reflection.resources[0].sampler; });
    rejectBoundary("reflection.resources[0].access", [](auto &artifact) -> auto & { return artifact.reflection.resources[0].access; });
    rejectBoundary("reflection.resources[0].bufferKind", [](auto &artifact) -> auto & { return artifact.reflection.resources[0].bufferKind; });
    rejectBoundary("reflection.blocks[0].members[0].scalarType",
                   [](auto &artifact) -> auto & { return artifact.reflection.blocks[0].members[0].scalarType; });
    rejectBoundary("reflection.blocks[0].members[0].kind", [](auto &artifact) -> auto & { return artifact.reflection.blocks[0].members[0].kind; });
    rejectBoundary("reflection.blocks[0].members[0].matrixLayout",
                   [](auto &artifact) -> auto & { return artifact.reflection.blocks[0].members[0].matrixLayout; });
    rejectBoundary("reflection.nativeBindings[0].category", [](auto &artifact) -> auto & { return artifact.reflection.nativeBindings[0].category; });
}

TEST(ShaderCacheStoreTest, SchemaEnumBoundsRejectInvalidOwnershipAndType)
{
    ShaderVariantSchema schema;
    schema.sources       = {{Name("material"), 0, 1}};
    schema.entries       = {{Name("OPTION"), Name("material"), 0, 1, 0}};
    schema.totalBits     = 1;
    const auto       key = TestCacheKey(schema);
    ShaderCacheIndex valid;
    ASSERT_TRUE(UpsertArtifact(valid, key.sourcePath, schema, key.schemaFp, key, TestArtifact(key), {}));
    ShaderCacheIndex decoded;
    ASSERT_TRUE(DecodeIndex(EncodeIndex(valid), decoded));
    const auto rejectBoundary = [&](const auto &field) {
        auto invalid = valid;
        using Enum   = std::remove_cvref_t<decltype(field(invalid.paths[0].second.schema.entries[0]))>;
        for (const auto value : {Enum::END, static_cast<Enum>(static_cast<uint32_t>(Enum::END) + 1), static_cast<Enum>(UINT8_MAX)}) {
            field(invalid.paths[0].second.schema.entries[0]) = value;
            invalid.paths[0].second.schemaFp                 = HashSchema(invalid.paths[0].second.schema);
            auto output                                      = valid;
            EXPECT_FALSE(DecodeIndex(EncodeIndex(invalid), output));
            EXPECT_EQ(output.paths[0].second.schemaFp, valid.paths[0].second.schemaFp);
        }
    };
    rejectBoundary([](auto &entry) -> auto & { return entry.domain; });
    rejectBoundary([](auto &entry) -> auto & { return entry.type; });
}

TEST(ShaderCacheStoreTest, VertexScalarSubsetStillRejectsBoolean)
{
    auto artifact              = TestArtifact(TestCacheKey());
    artifact.identity.stage    = ShaderStageFlagBit::VS;
    artifact.reflection.stages = ShaderStageFlags(ShaderStageFlagBit::VS);
    ShaderVertexInput input;
    input.semantic   = "POSITION";
    input.components = 3;
    for (const auto type : {ShaderScalarType::FLOAT, ShaderScalarType::INT, ShaderScalarType::UINT, ShaderScalarType::BOOL}) {
        input.scalarType                 = type;
        artifact.reflection.vertexInputs = {input};
        artifact.identity.interfaceFp    = ShaderInterfaceFingerprint(artifact.reflection);
        artifact.artifactId              = ComputeShaderArtifactId(artifact);
        ShaderCacheBlob output;
        EXPECT_EQ(DecodeBlob(EncodeBlob(artifact), output), type != ShaderScalarType::BOOL);
    }
}

TEST(ShaderCacheStoreTest, CorruptOrPreviousArtifactsNeverReplaceOutput)
{
    const auto      artifact = TestArtifact(TestCacheKey());
    const auto      bytes    = EncodeBlob(artifact);
    ShaderCacheBlob output   = artifact;
    for (size_t size = 0; size < bytes.size(); ++size) {
        EXPECT_FALSE(DecodeBlob({bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)}, output)) << size;
        EXPECT_EQ(output.artifactId, artifact.artifactId);
    }
    auto corrupt = bytes;
    corrupt.back() ^= 1;
    EXPECT_FALSE(DecodeBlob(corrupt, output));
    corrupt    = bytes;
    corrupt[4] = 5;
    EXPECT_FALSE(DecodeBlob(corrupt, output));
    corrupt = bytes;
    corrupt.push_back(0);
    EXPECT_FALSE(DecodeBlob(corrupt, output));
    auto invalid           = artifact;
    invalid.identity.stage = static_cast<ShaderStageFlagBit>(UINT32_MAX);
    EXPECT_FALSE(DecodeBlob(EncodeBlob(invalid), output));
    invalid                           = artifact;
    invalid.reflection.nativeBindings = {{0, 0, ShaderStageFlagBit::FS, 0}};
    EXPECT_FALSE(DecodeBlob(EncodeBlob(invalid), output));
}

TEST(ShaderCacheStoreTest, IndexRejectsCorruptionOversizedCountsAndOldFormat)
{
    const auto       key = TestCacheKey();
    ShaderCacheIndex index;
    ASSERT_TRUE(UpsertArtifact(index, key.sourcePath, {}, key.schemaFp, key, TestArtifact(key), {}));
    auto             bytes  = EncodeIndex(index);
    ShaderCacheIndex output = index;
    bytes.push_back(0);
    EXPECT_FALSE(DecodeIndex(bytes, output));
    bytes    = EncodeIndex(index);
    bytes[0] = 5;
    EXPECT_FALSE(DecodeIndex(bytes, output));
    bytes = EncodeIndex(index);
    std::fill(bytes.begin() + 4, bytes.begin() + 8, 255);
    EXPECT_FALSE(DecodeIndex(bytes, output));
    index.paths[0].second.artifacts[0].second.key.producer.version = "tampered";
    EXPECT_FALSE(DecodeIndex(EncodeIndex(index), output));
    EXPECT_EQ(output.paths[0].second.artifacts[0].second.key, key);
}

TEST(ShaderCacheStoreTest, BlobIsPublishedByArtifactIdBeforeIndex)
{
    ShaderCacheDirectory directory("store");
    const auto           key      = TestCacheKey();
    const auto           artifact = TestArtifact(key);
    ShaderBlobStore      store(directory.fs.Get(), directory.fs.Get());
    ASSERT_TRUE(store.Store(key, artifact)) << store.LastError();
    ShaderCompileResult output;
    EXPECT_FALSE(store.Load(key, output));
    ASSERT_TRUE(UpsertLocalArtifact(*directory.fs, "vulkan", key.sourcePath, {}, key.schemaFp, key, artifact, {}));
    ASSERT_TRUE(store.Load(key, output)) << store.LastError();
    EXPECT_EQ(output.artifactId, artifact.artifactId);
    auto changed             = key;
    changed.producer.version = "new";
    changed.toolchainFp      = ShaderProducerFingerprint(changed.producer);
    EXPECT_FALSE(store.Load(changed, output));
    auto record = ShaderCacheArtifact{CompileHash(key), artifact.artifactId, key, artifact.identity, {}};
    record.artifactId ^= 1;
    EXPECT_FALSE(store.LoadArtifact(record, output));
    EXPECT_EQ(output.artifactId, artifact.artifactId);
}

TEST(ShaderCacheStoreTest, BackendPlatformAndProducerAreRecordScoped)
{
    ShaderCacheDirectory directory("targets");
    ShaderCacheIndex     metalIndex;
    for (const auto platform : {ShaderPlatform::MACOS, ShaderPlatform::IOS}) {
        const auto key      = TestCacheKey({}, "s.slang", ShaderTarget::MSL, platform);
        const auto artifact = TestArtifact(key);
        ASSERT_TRUE(ShaderBlobStore(directory.fs.Get(), directory.fs.Get()).Store(key, artifact));
        ASSERT_TRUE(UpsertArtifact(metalIndex, key.sourcePath, {}, key.schemaFp, key, artifact, {}));
    }
    ASSERT_EQ(metalIndex.paths[0].second.artifacts.size(), 2u);
    ASSERT_TRUE(SaveIndex(*directory.fs, "metal", metalIndex));
    for (const auto platform : {ShaderPlatform::MACOS, ShaderPlatform::IOS}) {
        const auto          key = TestCacheKey({}, "s.slang", ShaderTarget::MSL, platform);
        ShaderCompileResult result;
        ASSERT_TRUE(ShaderBlobStore(directory.fs.Get(), nullptr).Load(key, result));
        EXPECT_EQ(result.identity.environment.platform, platform);
    }
}

TEST(ShaderCacheStoreTest, FullIdentityIsCheckedAfterHashMatch)
{
    const auto       key = TestCacheKey();
    ShaderCacheIndex index;
    ASSERT_TRUE(UpsertArtifact(index, key.sourcePath, {}, key.schemaFp, key, TestArtifact(key), {}));
    auto &record           = index.paths[0].second.artifacts[0].second;
    record.key.sourceEntry = "different";
    record.compileHash     = CompileHash(key);
    EXPECT_EQ(FindArtifact(index.paths[0].second, key), nullptr);
}

TEST(ShaderCacheStoreTest, StableNamesAndCanonicalSpecializationInputs)
{
    ShaderVariantSchema schema;
    schema.sources   = {{Name("material"), 0, 8}};
    schema.entries   = {{Name("Z_MACRO"), Name("material"), 0, 1, 1}, {Name("A_SPEC"), Name("material"), 1, 1, 0}};
    schema.totalBits = 8;
    ShaderVariant variant;
    variant.entries = {{Name("A_SPEC"), 1}, {Name("Z_MACRO"), 0}};
    std::vector<ShaderVariantValue>         macros;
    const std::vector<ShaderVariantBinding> native{{"A_SPEC", "kSpec", ShaderVariantType::UINT32, ShaderVariantLowering::NATIVE, 3, 0}};
    ASSERT_TRUE(CanonicalShaderVariants(&variant, schema, ShaderTarget::MSL, macros, nullptr, &native));
    EXPECT_EQ(macros, (std::vector<ShaderVariantValue>{{"Z_MACRO", 0}}));
    auto same = variant;
    std::reverse(same.entries.begin(), same.entries.end());
    EXPECT_EQ(variant.ContentHash(), same.ContentHash());
    ASSERT_TRUE(CanonicalShaderVariants(&variant, schema, ShaderTarget::DXIL, macros));
    EXPECT_EQ(macros, (std::vector<ShaderVariantValue>{{"A_SPEC", 1}, {"Z_MACRO", 0}}));
    same.entries.push_back(same.entries[0]);
    EXPECT_FALSE(CanonicalShaderVariants(&same, schema, ShaderTarget::MSL, macros));
    auto key                         = TestCacheKey(schema);
    auto changed                     = key;
    changed.environment.matrixLayout = ShaderMatrixLayout::ROW_MAJOR;
    EXPECT_NE(CompileHash(key), CompileHash(changed));
    changed                        = key;
    changed.producer.configuration = 1;
    EXPECT_NE(CompileHash(key), CompileHash(changed));
}
