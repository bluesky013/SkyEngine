#include <core/archive/MemoryStreamArchive.h>
#include <gtest/gtest.h>
#include <particle/assets/ParticleEffectAsset.h>

using namespace sky;
using namespace sky::particle;

namespace {
    ParticleEffectAssetData MakeAsset()
    {
        ParticleEffectDesc effect;
        effect.fixedBounds = {{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}};
        ParticleEmitterDesc emitter;
        emitter.name       = Name("sparks");
        emitter.capacity   = 32;
        emitter.spawn.rate = 4.f;
        ParticleModuleDesc initialize;
        initialize.type = ParticleModuleType::INITIALIZE;
        initialize.parameters.push_back({Name("lifetime"), ParticleValue{.constant = 2.f}});
        emitter.modules.push_back(initialize);
        emitter.renderers.push_back(
            {ParticleRendererType::SPRITE, ParticleBlendMode::ALPHA, false, false, Uuid::CreateFromString("00000000-0000-0000-0000-000000000002")});
        effect.emitters.push_back(emitter);
        const auto              compiled = CompileParticleEffect(effect);
        ParticleEffectAssetData data;
        data.sourceHash = 0x1122334455667788ull;
        data.programs   = compiled.programs;
        data.valid      = compiled.succeeded;
        return data;
    }
} // namespace

TEST(ParticleEffectAsset, BinaryRoundTripPreservesCompiledProgram)
{
    const auto          source = MakeAsset();
    OMemoryArchive      output;
    BinaryOutputArchive writer(output);
    source.Save(writer);
    ASSERT_TRUE(writer.IsValid());

    IMemoryArchive          input(output.Data(), output.Size());
    BinaryInputArchive      reader(input);
    ParticleEffectAssetData decoded;
    decoded.Load(reader);
    ASSERT_TRUE(decoded.IsLoadValid());
    EXPECT_EQ(decoded.sourceHash, source.sourceHash);
    ASSERT_EQ(decoded.programs.size(), source.programs.size());
    EXPECT_EQ(decoded.programs.front().hash, source.programs.front().hash);
    EXPECT_TRUE(decoded.programs.front().emitter == source.programs.front().emitter);
    EXPECT_EQ(decoded.programs.front().layout.stride, source.programs.front().layout.stride);
    EXPECT_EQ(decoded.programs.front().renderers.size(), 1u);
    EXPECT_EQ(decoded.programs.front().renderers.front().texture.ToString(), "00000000-0000-0000-0000-000000000002");
}

TEST(ParticleEffectAsset, RejectsTruncatedAndLegacyProducts)
{
    const auto          source = MakeAsset();
    OMemoryArchive      output;
    BinaryOutputArchive writer(output);
    source.Save(writer);

    IMemoryArchive          shortInput(output.Data(), output.Size() - 3);
    BinaryInputArchive      shortReader(shortInput);
    ParticleEffectAssetData decoded;
    decoded.Load(shortReader);
    EXPECT_FALSE(decoded.IsLoadValid());
    EXPECT_TRUE(decoded.programs.empty());

    IMemoryArchive     legacy(reinterpret_cast<const char *>(&source.version), sizeof(source.version));
    BinaryInputArchive legacyReader(legacy);
    decoded.Load(legacyReader);
    EXPECT_FALSE(decoded.IsLoadValid());
}

TEST(ParticleEffectAsset, RoundTripsRandomInitializationAndLocalSpace)
{
    auto  source                = MakeAsset();
    auto &program               = source.programs.front();
    program.space               = ParticleSimSpace::LOCAL;
    auto &initialize            = program.spawnModules.front();
    initialize.randomMask       = 2;
    initialize.values[1]        = .2f;
    initialize.randomMaximum[1] = .5f;
    initialize.position         = {1.f, 2.f, 3.f};
    initialize.velocity         = {4.f, 5.f, 6.f};
    program.hash                = HashParticleProgram(program);
    OMemoryArchive      output;
    BinaryOutputArchive writer(output);
    source.Save(writer);
    IMemoryArchive          input(output.Data(), output.Size());
    BinaryInputArchive      reader(input);
    ParticleEffectAssetData decoded;
    decoded.Load(reader);
    ASSERT_TRUE(decoded.IsLoadValid());
    const auto &result = decoded.programs.front();
    EXPECT_EQ(result.space, ParticleSimSpace::LOCAL);
    EXPECT_EQ(result.hash, program.hash);
    EXPECT_EQ(result.spawnModules.front().randomMask, 2u);
    EXPECT_FLOAT_EQ(result.spawnModules.front().randomMaximum[1], .5f);
    EXPECT_FLOAT_EQ(result.spawnModules.front().position.y, 2.f);
    EXPECT_FLOAT_EQ(result.spawnModules.front().velocity.z, 6.f);
}

TEST(ParticleEffectAsset, RejectsInvalidCompiledFieldsEvenWhenArchiveIsComplete)
{
    for (const auto invalidSpace : {false, true}) {
        auto source = MakeAsset();
        if (invalidSpace)
            source.programs.front().space = ParticleSimSpace::END;
        else
            source.programs.front().spawn.rate = -1.f;
        source.programs.front().hash = HashParticleProgram(source.programs.front());
        OMemoryArchive      output;
        BinaryOutputArchive writer(output);
        source.Save(writer);
        IMemoryArchive          input(output.Data(), output.Size());
        BinaryInputArchive      reader(input);
        ParticleEffectAssetData decoded;
        decoded.Load(reader);
        EXPECT_FALSE(decoded.IsLoadValid());
    }
}

TEST(ParticleEffectAsset, RejectsVersionTwoProducts)
{
    auto  source  = MakeAsset();
    auto &program = source.programs.front();
    OMemoryArchive      output;
    BinaryOutputArchive writer(output);
    writer.SaveValue(uint32_t{0x50454350});
    writer.SaveValue(uint32_t{2});
    writer.SaveValue(source.version);
    writer.SaveValue(source.sourceHash);
    writer.SaveValue(uint32_t{1});
    writer.SaveValue(uint64_t{123});
    writer.SaveValue(program.emitter.GetStr());
    IMemoryArchive     input(output.Data(), output.Size());
    BinaryInputArchive reader(input);
    ParticleEffectAssetData decoded;
    decoded.Load(reader);
    EXPECT_FALSE(decoded.IsLoadValid());
    EXPECT_TRUE(decoded.programs.empty());
}
