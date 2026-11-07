#include <gtest/gtest.h>
#include <particle/ParticleProgram.h>
#include <particle/assets/ParticleEffectSource.h>

using namespace sky::particle;

TEST(ParticleEffectSource, ParsesAndCompilesSpriteEffect)
{
    const char        *text = R"({
        "version": 1,
        "emitters": [{
            "name": "sparks",
            "simTarget": "CPU",
            "capacity": 64,
            "spawn": {"rate": 8, "burstInterval": 0, "burstCount": 0},
            "modules": [
                {"type": "INITIALIZE", "params": {"lifetime": 2, "size": 0.5}},
                {"type": "GRAVITY", "params": {"gravity": [0, -9.81, 0]}},
                {"type": "SIZE_OVER_LIFE", "params": {"size": [{"time": 0, "value": 1}, {"time": 1, "value": 0.2}]}}
            ],
            "renderers": [{"type": "SPRITE", "blend": "ALPHA", "lit": false, "soft": false}]
        }]
    })";
    ParticleEffectDesc effect;
    std::string        error;
    ASSERT_TRUE(ParseParticleSource(text, effect, error)) << error;
    ASSERT_EQ(effect.emitters.size(), 1u);
    EXPECT_EQ(effect.emitters.front().capacity, 64u);
    EXPECT_EQ(effect.emitters.front().modules.size(), 3u);
    const auto compiled = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);
    ASSERT_EQ(compiled.programs.size(), 1u);
    EXPECT_TRUE((uint32_t)compiled.programs.front().flags & (uint32_t)ParticleProgramFlags::GRAVITY);
}

TEST(ParticleEffectSource, RejectsMalformedSourcesAndRoundTripsNames)
{
    ParticleEffectDesc effect;
    std::string        error;
    EXPECT_FALSE(ParseParticleSource("{}", effect, error));
    EXPECT_FALSE(ParseParticleSource(R"({"emitters":[{"name":"a"}]})", effect, error));
    const char *text = R"({"version":1,"emitters":[{"name":"sparks","simTarget":"CPU","capacity":8,"modules":[],"renderers":[]}]})";
    ASSERT_TRUE(ParseParticleSource(text, effect, error)) << error;
    std::string written;
    ASSERT_TRUE(WriteParticleSource(effect, written));
    ParticleEffectDesc roundtrip;
    ASSERT_TRUE(ParseParticleSource(written, roundtrip, error)) << error;
    EXPECT_TRUE(roundtrip.emitters.front().name == effect.emitters.front().name);
    const auto before = CompileParticleEffect(effect);
    const auto after  = CompileParticleEffect(roundtrip);
    ASSERT_TRUE(before.succeeded);
    ASSERT_TRUE(after.succeeded);
    ASSERT_FALSE(before.programs.empty());
    EXPECT_EQ(before.programs.front().hash, after.programs.front().hash);
}

TEST(ParticleEffectSource, RoundTripsParametersCurvesAndBounds)
{
    const char        *text = R"({
        "version": 1,
        "fixedBounds": {"min": [-2, -2, -2], "max": [2, 2, 2]},
        "userParameters": [{"name": "wind", "value": [1, 0, 0]}],
        "emitters": [{
            "name": "sparks", "simTarget": "CPU", "capacity": 64,
            "spawn": {"rate": 8},
            "modules": [
                {"type": "INITIALIZE", "params": {"lifetime": 2, "size": {"min": 0.2, "max": 0.5}}},
                {"type": "COLOR_OVER_LIFE", "params": {"color": [{"time": 0, "color": [1, 1, 1, 1]}, {"time": 1, "color": [1, 0, 0, 0]}]}},
                {"type": "SIZE_OVER_LIFE", "params": {"size": [{"time": 0, "value": 1}, {"time": 1, "value": 0.1}]}}
            ],
            "renderers": [{"type": "SPRITE", "blend": "ADDITIVE", "lit": false, "soft": false, "texture": "00000000-0000-0000-0000-000000000002"}]
        }]
    })";
    ParticleEffectDesc effect;
    std::string        error;
    ASSERT_TRUE(ParseParticleSource(text, effect, error)) << error;
    std::string written;
    ASSERT_TRUE(WriteParticleSource(effect, written));
    ParticleEffectDesc roundtrip;
    ASSERT_TRUE(ParseParticleSource(written, roundtrip, error)) << error;
    ASSERT_EQ(roundtrip.userParameters.size(), 1u);
    EXPECT_FLOAT_EQ(roundtrip.fixedBounds.max.x, 2.f);
    ASSERT_EQ(roundtrip.emitters.front().modules.size(), 3u);
    EXPECT_EQ(roundtrip.emitters.front().renderers.front().texture.ToString(), "00000000-0000-0000-0000-000000000002");
    const auto before = CompileParticleEffect(effect);
    const auto after  = CompileParticleEffect(roundtrip);
    ASSERT_TRUE(before.succeeded);
    ASSERT_TRUE(after.succeeded);
    EXPECT_EQ(before.programs.front().hash, after.programs.front().hash);
}

TEST(ParticleEffectSource, RejectsWrongTypesWithoutOverwritingValidOutput)
{
    ParticleEffectDesc effect;
    std::string        error;
    const char        *valid = R"({"emitters":[{"name":"retained","simTarget":"CPU","capacity":8}]})";
    ASSERT_TRUE(ParseParticleSource(valid, effect, error));
    for (const auto *invalid :
         {R"({"emitters":[{"name":"a","simTarget":"CPU","capacity":8,"spawn":[]}]})",
          R"({"emitters":[{"name":"a","simTarget":"TYPO","capacity":8}]})",
          R"({"emitters":[{"name":"a","simTarget":"CPU","capacity":8,"spawn":{"rate":1e100}}]})",
          R"({"emitters":[{"name":"a","simTarget":"CPU","capacity":8,"renderers":[{"type":"TYPO","blend":"ALPHA"}]}]})",
          R"({"emitters":[{"name":"a","simTarget":"CPU","capacity":8,"renderers":[{"type":"SPRITE","blend":"ALPHA","lit":1}]}]})"}) {
        EXPECT_FALSE(ParseParticleSource(invalid, effect, error));
        EXPECT_EQ(effect.emitters.front().name.GetStr(), "retained");
        EXPECT_FALSE(error.empty());
    }
}

TEST(ParticleEffectSource, RoundTripsSimulationSpaceWarmupModuleNameAndAllValueFields)
{
    ParticleEffectDesc                 effect;
    sky::particle::ParticleEmitterDesc emitter;
    emitter.name                = sky::Name("roundtrip");
    emitter.capacity            = 8;
    emitter.space               = ParticleSimSpace::LOCAL;
    emitter.spawn.warmupSeconds = 2.f;
    emitter.bounds              = {{-1.f, -2.f, -3.f}, {1.f, 2.f, 3.f}};
    ParticleModuleDesc module;
    module.type = ParticleModuleType::INITIALIZE;
    module.name = sky::Name("Spawn Settings");
    ParticleValue size;
    size.kind  = ParticleValueKind::RANDOM;
    size.range = {.2f, .5f};
    module.parameters.push_back({sky::Name("size"), size});
    emitter.modules.push_back(module);
    effect.emitters.push_back(emitter);
    ParticleValue parameter;
    parameter.kind       = ParticleValueKind::ATTRIBUTE;
    parameter.attribute  = ParticleAttribute::VELOCITY;
    parameter.parameter  = sky::Name("wind");
    parameter.vector     = {1.f, 2.f, 3.f};
    parameter.curve.keys = {{.5f, {1.f, 2.f, 3.f}}};
    effect.userParameters.push_back({sky::Name("reference"), parameter});
    std::string text, error;
    ASSERT_TRUE(WriteParticleSource(effect, text));
    ParticleEffectDesc decoded;
    ASSERT_TRUE(ParseParticleSource(text, decoded, error)) << error;
    const auto &result = decoded.emitters.front();
    EXPECT_EQ(result.space, ParticleSimSpace::LOCAL);
    EXPECT_FLOAT_EQ(result.spawn.warmupSeconds, 2.f);
    EXPECT_FLOAT_EQ(result.bounds.max.y, 2.f);
    EXPECT_EQ(result.modules.front().name.GetStr(), "Spawn Settings");
    EXPECT_EQ(result.modules.front().parameters.front().value.kind, ParticleValueKind::RANDOM);
    const auto &value = decoded.userParameters.front().value;
    EXPECT_EQ(value.kind, ParticleValueKind::ATTRIBUTE);
    EXPECT_EQ(value.attribute, ParticleAttribute::VELOCITY);
    EXPECT_EQ(value.parameter.GetStr(), "wind");
    EXPECT_FLOAT_EQ(value.curve.keys.front().value.z, 3.f);
}
