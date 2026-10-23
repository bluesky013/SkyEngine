#include <aurora/shader/ShaderCacheStore.h>

#include "ShaderArtifactTest.h"

#include <algorithm>
#include <core/file/MultiFileSystem.h>
#include <gtest/gtest.h>

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
    schema.entries   = {{Name("Z_MACRO"), Name("material"), 0, 1, 1}, {Name("A_SPEC"), Name("material"), 1, 1, 0, true, 3}};
    schema.totalBits = 8;
    ShaderVariant variant;
    variant.entries = {{Name("A_SPEC"), 1}, {Name("Z_MACRO"), 0}};
    std::vector<ShaderVariantValue> macros;
    ASSERT_TRUE(CanonicalShaderVariants(&variant, schema, ShaderTarget::MSL, macros));
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
