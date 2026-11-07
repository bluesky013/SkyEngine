#include <gtest/gtest.h>
#include <particle/ParticleProgram.h>
#include <particle/ParticleSolver.h>

#include <chrono>
#include <cmath>
#include <iostream>

using namespace sky;
using namespace sky::particle;

namespace {
    ParticleEffectDesc MakeEffect()
    {
        ParticleEffectDesc effect;
        effect.fixedBounds = {{-10.f, -10.f, -10.f}, {10.f, 10.f, 10.f}};
        ParticleEmitterDesc emitter;
        emitter.name       = Name("sparks");
        emitter.capacity   = 128;
        emitter.spawn.rate = 10.f;
        ParticleModuleDesc initialize;
        initialize.type = ParticleModuleType::INITIALIZE;
        initialize.parameters.push_back({Name("lifetime"), ParticleValue{.kind = ParticleValueKind::CONSTANT, .constant = 2.f}});
        initialize.parameters.push_back({Name("size"), ParticleValue{.kind = ParticleValueKind::CONSTANT, .constant = 0.5f}});
        emitter.modules.push_back(initialize);
        ParticleModuleDesc gravity;
        gravity.type = ParticleModuleType::GRAVITY;
        gravity.parameters.push_back({Name("gravity"), ParticleValue{.kind = ParticleValueKind::CONSTANT, .vector = {0.f, -9.81f, 0.f}}});
        emitter.modules.push_back(gravity);
        ParticleModuleDesc drag;
        drag.type = ParticleModuleType::DRAG;
        drag.parameters.push_back({Name("drag"), ParticleValue{.kind = ParticleValueKind::CONSTANT, .constant = 0.1f}});
        emitter.modules.push_back(drag);
        ParticleRendererDesc renderer;
        renderer.type  = ParticleRendererType::SPRITE;
        renderer.blend = ParticleBlendMode::ALPHA;
        emitter.renderers.push_back(renderer);
        effect.emitters.push_back(std::move(emitter));
        return effect;
    }
} // namespace

TEST(ParticleCompiler, CompilesStableProgram)
{
    const auto effect = MakeEffect();
    const auto first  = CompileParticleEffect(effect);
    const auto second = CompileParticleEffect(effect);
    ASSERT_TRUE(first.succeeded) << first.diagnostics.front().message;
    ASSERT_EQ(first.programs.size(), 1u);
    const auto &program = first.programs.front();
    EXPECT_EQ(program.capacity, 128u);
    EXPECT_TRUE(program.emitter == Name("sparks"));
    EXPECT_EQ(program.spawnModules.size(), 1u);
    EXPECT_EQ(program.renderers.size(), 1u);
    EXPECT_EQ(program.layout.offsets[(size_t)ParticleAttribute::POSITION], 0u);
    EXPECT_EQ(program.layout.offsets[(size_t)ParticleAttribute::VELOCITY], 12u);
    EXPECT_EQ(program.layout.offsets[(size_t)ParticleAttribute::COLOR], 32u);
    EXPECT_TRUE((uint32_t)program.flags & (uint32_t)ParticleProgramFlags::GRAVITY);
    EXPECT_TRUE((uint32_t)program.flags & (uint32_t)ParticleProgramFlags::DRAG);
    EXPECT_EQ(program.hash, second.programs.front().hash);
}

TEST(ParticleCompiler, RejectsInvalidCapacityAndRenderers)
{
    auto effect                      = MakeEffect();
    effect.emitters.front().capacity = 0;
    auto result                      = CompileParticleEffect(effect);
    EXPECT_FALSE(result.succeeded);
    EXPECT_FALSE(result.diagnostics.empty());

    effect                                          = MakeEffect();
    effect.emitters.front().renderers.front().blend = ParticleBlendMode::END;
    result                                          = CompileParticleEffect(effect);
    EXPECT_FALSE(result.succeeded);
    EXPECT_FALSE(result.diagnostics.empty());

    effect                                     = MakeEffect();
    effect.emitters.front().renderers.front().lit = true;
    result                                     = CompileParticleEffect(effect);
    EXPECT_FALSE(result.succeeded);

    effect                                      = MakeEffect();
    effect.emitters.front().renderers.front().soft = true;
    result                                      = CompileParticleEffect(effect);
    EXPECT_FALSE(result.succeeded);

    effect = MakeEffect();
    ParticleModuleDesc bound;
    bound.type = ParticleModuleType::GRAVITY;
    bound.parameters.push_back({Name("gravity"), ParticleValue{.kind = ParticleValueKind::PARAMETER}});
    effect.emitters.front().modules.push_back(std::move(bound));
    result = CompileParticleEffect(effect);
    EXPECT_FALSE(result.succeeded);
}

TEST(ParticleSolver, SpawnsAgesAndKillsDeterministically)
{
    const auto compiled = CompileParticleEffect(MakeEffect());
    ASSERT_TRUE(compiled.succeeded);
    ParticleSolver     first, second;
    ParticleSolverDesc desc;
    desc.randomSeed = 42;
    ASSERT_TRUE(first.Init(compiled.programs.front(), desc));
    ASSERT_TRUE(second.Init(compiled.programs.front(), desc));
    first.Step(0.1f);
    second.Step(0.1f);
    ASSERT_EQ(first.GetStorage().aliveCount, 1u);
    ASSERT_EQ(second.GetStorage().aliveCount, 1u);
    const auto slot = first.GetStorage().alive.front();
    EXPECT_FLOAT_EQ(first.GetStorage().ages[slot], 0.1f);
    EXPECT_LT(first.GetStorage().velocities[slot].y, 0.f);
    EXPECT_FLOAT_EQ(first.GetStorage().velocities[slot].y, second.GetStorage().velocities[slot].y);
    EXPECT_FLOAT_EQ(first.GetStorage().positions[slot].y, second.GetStorage().positions[slot].y);
    auto killProgram       = compiled.programs.front();
    killProgram.spawn.rate = 0.f;
    ParticleSolver killer;
    ASSERT_TRUE(killer.Init(killProgram, desc));
    killer.Step(0.1f, 1);
    ASSERT_EQ(killer.GetStorage().aliveCount, 1u);
    killer.Step(2.5f);
    EXPECT_EQ(killer.GetStorage().aliveCount, 0u);
}

TEST(ParticleSolver, RejectsUninitializedProgram)
{
    ParticleSolver solver;
    EXPECT_FALSE(solver.Init(ParticleProgram{}));
}

TEST(ParticleSolver, ChunkedUpdateMatchesSerialForLargeBatches)
{
    auto effect                        = MakeEffect();
    effect.emitters.front().capacity   = 4096;
    effect.emitters.front().spawn.rate = 1024.f;
    const auto compiled                = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);
    ParticleSolver     first, second;
    ParticleSolverDesc desc;
    desc.randomSeed = 99;
    ASSERT_TRUE(first.Init(compiled.programs.front(), desc));
    ASSERT_TRUE(second.Init(compiled.programs.front(), desc));
    first.Step(1.f);
    second.Step(1.f);
    ASSERT_EQ(first.GetStorage().aliveCount, 1024u);
    for (size_t i = 0; i < first.GetStorage().alive.size(); i += 97) {
        const auto slot = first.GetStorage().alive[i];
        EXPECT_FLOAT_EQ(first.GetStorage().positions[slot].x, second.GetStorage().positions[slot].x);
        EXPECT_FLOAT_EQ(first.GetStorage().positions[slot].y, second.GetStorage().positions[slot].y);
        EXPECT_FLOAT_EQ(first.GetStorage().velocities[slot].y, second.GetStorage().velocities[slot].y);
    }
}

TEST(ParticleCompiler, TextureParticipatesInProgramIdentity)
{
    auto first                                        = MakeEffect();
    auto second                                       = first;
    second.emitters.front().renderers.front().texture = Uuid::CreateFromString("00000000-0000-0000-0000-000000000002");
    const auto a                                      = CompileParticleEffect(first);
    const auto b                                      = CompileParticleEffect(second);
    ASSERT_TRUE(a.succeeded);
    ASSERT_TRUE(b.succeeded);
    EXPECT_NE(a.programs.front().hash, b.programs.front().hash);
}

TEST(ParticleSolver, SubUvCurveWritesFrameIndex)
{
    auto effect                        = MakeEffect();
    effect.emitters.front().spawn.rate = 0.f;
    ParticleModuleDesc subUv;
    subUv.type = ParticleModuleType::SUB_UV;
    subUv.parameters.push_back({Name("columns"), ParticleValue{.constant = 2.f}});
    subUv.parameters.push_back({Name("rows"), ParticleValue{.constant = 2.f}});
    ParticleValue frame;
    frame.kind       = ParticleValueKind::CURVE;
    frame.curve.keys = {{0.f, {0.f, 0.f, 0.f}}, {1.f, {3.f, 0.f, 0.f}}};
    subUv.parameters.push_back({Name("frame"), frame});
    effect.emitters.front().modules.push_back(subUv);
    const auto compiled = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);
    ParticleSolver solver;
    ASSERT_TRUE(solver.Init(compiled.programs.front()));
    solver.Step(1.f, 1);
    ASSERT_EQ(solver.GetStorage().aliveCount, 1u);
    const auto slot = solver.GetStorage().alive.front();
    solver.Step(1.f);
    EXPECT_NEAR(solver.GetStorage().subUv[slot], 3.f, 0.01f);
}

TEST(ParticleSolver, BurstWarmupAndBudgetAreBounded)
{
    auto effect                                 = MakeEffect();
    effect.emitters.front().capacity            = 256;
    effect.emitters.front().spawn.rate          = 0.f;
    effect.emitters.front().spawn.warmupSeconds = 1.f;
    ParticleModuleDesc burst;
    burst.type = ParticleModuleType::BURST;
    burst.parameters.push_back({Name("interval"), ParticleValue{.constant = .5f}});
    burst.parameters.push_back({Name("count"), ParticleValue{.constant = 3.f}});
    effect.emitters.front().modules.push_back(burst);
    const auto compiled = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);
    auto program       = compiled.programs.front();
    program.spawn.rate = 30.f;
    ParticleSolver solver;
    ASSERT_TRUE(solver.Init(program));
    EXPECT_GT(solver.GetStorage().aliveCount, 0u);
    solver.Step(1.f);
    EXPECT_LE(solver.GetStorage().aliveCount, solver.GetStorage().capacity);
    solver.KillExcess(10);
    EXPECT_EQ(solver.GetStorage().aliveCount, 10u);
}

TEST(ParticleSolver, EmitsSpawnAndDeathEvents)
{
    auto effect                        = MakeEffect();
    effect.emitters.front().spawn.rate = 0.f;
    const auto compiled                = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);
    ParticleSolver solver;
    ASSERT_TRUE(solver.Init(compiled.programs.front()));
    solver.Step(0.1f, 1);
    auto events = solver.DrainEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().type, ParticleEventType::SPAWN);
    solver.Step(2.1f);
    events = solver.DrainEvents();
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.front().type, ParticleEventType::DEATH);
}

TEST(ParticleSolver, RateScaleReducesSpawnRate)
{
    auto effect                        = MakeEffect();
    effect.emitters.front().capacity   = 256;
    effect.emitters.front().spawn.rate = 10.f;
    const auto compiled                = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);
    ParticleSolver solver;
    ASSERT_TRUE(solver.Init(compiled.programs.front()));
    solver.SetRateScale(.5f);
    solver.Step(1.f);
    EXPECT_EQ(solver.GetStorage().aliveCount, 5u);
}

TEST(ParticleSolver, HandlesHundredThousandParticleBudget)
{
    auto effect                        = MakeEffect();
    effect.emitters.front().capacity   = 100000;
    effect.emitters.front().spawn.rate = 100000.f;
    const auto compiled                = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);
    ParticleSolver solver;
    ASSERT_TRUE(solver.Init(compiled.programs.front()));
    const auto start = std::chrono::steady_clock::now();
    solver.Step(1.f);
    for (uint32_t i = 0; i < 8; ++i)
        solver.Step(1.f / 60.f);
    const auto elapsed = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "[ParticleBenchmark] 100k spawn + 8 updates: " << elapsed << " ms\n";
    EXPECT_EQ(solver.GetStorage().aliveCount, 100000u);
    EXPECT_LT(elapsed, 10000.f);
}

TEST(ParticleCompiler, RendererVariantKeyDistinguishesConfiguration)
{
    ParticleCompiledRenderer a;
    a.type                     = ParticleRendererType::SPRITE;
    a.blend                    = ParticleBlendMode::ALPHA;
    ParticleCompiledRenderer b = a;
    b.blend                    = ParticleBlendMode::ADDITIVE;
    ParticleCompiledRenderer c = a;
    c.texture                  = Uuid::CreateFromString("00000000-0000-0000-0000-000000000002");
    EXPECT_NE(RendererVariantKey(a), RendererVariantKey(b));
    EXPECT_NE(RendererVariantKey(a), RendererVariantKey(c));
    EXPECT_EQ(RendererVariantKey(a), RendererVariantKey(a));
}

TEST(ParticleCompiler, HashIncludesCurvesBoundsAndSimulationSpace)
{
    auto               effect = MakeEffect();
    ParticleModuleDesc size;
    size.type = ParticleModuleType::SIZE_OVER_LIFE;
    ParticleValue value;
    value.kind       = ParticleValueKind::CURVE;
    value.curve.keys = {{0.f, {1.f, 0.f, 0.f}}, {1.f, {2.f, 0.f, 0.f}}};
    size.parameters.push_back({Name("size"), value});
    effect.emitters.front().modules.push_back(size);
    const auto first = CompileParticleEffect(effect);
    ASSERT_TRUE(first.succeeded);
    effect.emitters.front().modules.back().parameters.front().value.curve.keys.back().value.x = 3.f;
    auto next                                                                                 = CompileParticleEffect(effect);
    ASSERT_TRUE(next.succeeded);
    EXPECT_NE(first.programs.front().hash, next.programs.front().hash);
    const auto previous           = next.programs.front().hash;
    effect.emitters.front().space = ParticleSimSpace::LOCAL;
    next                          = CompileParticleEffect(effect);
    ASSERT_TRUE(next.succeeded);
    EXPECT_NE(previous, next.programs.front().hash);
    effect.emitters.front().modules.back().parameters.front().value.kind = ParticleValueKind::PARAMETER;
    next                                                                 = CompileParticleEffect(effect);
    EXPECT_FALSE(next.succeeded);
    EXPECT_TRUE(next.programs.empty());
}

TEST(ParticleSolver, RandomInitializationUsesSeedAndPreservesRange)
{
    auto effect                        = MakeEffect();
    effect.emitters.front().spawn.rate = 0.f;
    auto &size                         = effect.emitters.front().modules.front().parameters[1].value;
    size.kind                          = ParticleValueKind::RANDOM;
    size.range                         = {.2f, .5f};
    const auto compiled                = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);
    ParticleSolver first, second;
    ASSERT_TRUE(first.Init(compiled.programs.front()));
    ASSERT_TRUE(second.Init(compiled.programs.front()));
    first.Step(.01f, 32);
    second.Step(.01f, 32);
    for (const auto slot : first.GetStorage().alive) {
        EXPECT_GE(first.GetStorage().sizes[slot], .2f);
        EXPECT_LE(first.GetStorage().sizes[slot], .5f);
        EXPECT_EQ(first.GetStorage().sizes[slot], second.GetStorage().sizes[slot]);
    }
    EXPECT_NE(first.GetStorage().sizes[0], first.GetStorage().sizes[1]);
}

TEST(ParticleSolver, AuthoredUpdateOrderAndRepeatedModulesArePreserved)
{
    auto effect                        = MakeEffect();
    effect.emitters.front().spawn.rate = 0.f;
    auto reversed                      = effect;
    std::swap(reversed.emitters.front().modules[1], reversed.emitters.front().modules[2]);
    ParticleSolver first, second;
    ASSERT_TRUE(first.Init(CompileParticleEffect(effect).programs.front()));
    ASSERT_TRUE(second.Init(CompileParticleEffect(reversed).programs.front()));
    first.Step(.1f, 1);
    second.Step(.1f, 1);
    EXPECT_NE(first.GetStorage().velocities[0].y, second.GetStorage().velocities[0].y);
    effect.emitters.front().modules.push_back(effect.emitters.front().modules[1]);
    ParticleSolver repeated;
    ASSERT_TRUE(repeated.Init(CompileParticleEffect(effect).programs.front()));
    repeated.Step(.1f, 1);
    EXPECT_LT(repeated.GetStorage().velocities[0].y, first.GetStorage().velocities[0].y);
}

TEST(ParticleSolver, SpawnTransformAppliesDuringWarmupOnlyInWorldSpace)
{
    auto effect                                 = MakeEffect();
    effect.emitters.front().spawn.warmupSeconds = .1f;
    Matrix4 transform                           = Matrix4::Identity();
    transform.Translate({5.f, 0.f, 0.f});
    ParticleSolver world, local;
    ASSERT_TRUE(world.Init(CompileParticleEffect(effect).programs.front(), {}, transform));
    effect.emitters.front().space = ParticleSimSpace::LOCAL;
    ASSERT_TRUE(local.Init(CompileParticleEffect(effect).programs.front(), {}, transform));
    ASSERT_FALSE(world.GetStorage().alive.empty());
    EXPECT_FLOAT_EQ(world.GetStorage().positions[world.GetStorage().alive.front()].x, 5.f);
    EXPECT_FLOAT_EQ(local.GetStorage().positions[local.GetStorage().alive.front()].x, 0.f);
}

TEST(ParticleSolver, DeathSweepReusesAllSlotsAndDrainsWithoutDiscardingOtherEmitters)
{
    auto effect                        = MakeEffect();
    effect.emitters.front().capacity   = 20000;
    effect.emitters.front().spawn.rate = 0.f;
    ParticleSolver solver;
    ASSERT_TRUE(solver.Init(CompileParticleEffect(effect).programs.front()));
    solver.Step(.01f, 20000);
    std::vector<ParticleEvent> events;
    solver.DrainEvents(events);
    const auto spawnEvents = events.size();
    solver.Step(3.f);
    EXPECT_EQ(solver.GetStorage().aliveCount, 0u);
    EXPECT_EQ(solver.GetStorage().freeList.size(), 20000u);
    solver.DrainEvents(events);
    EXPECT_GT(events.size(), spawnEvents);
    const auto count = events.size();
    solver.DrainEvents(events);
    EXPECT_EQ(events.size(), count);
    solver.Step(.01f, 20000);
    EXPECT_EQ(solver.GetStorage().aliveCount, 20000u);
}

TEST(ParticleSolver, SpawnWorkIsBoundedByFreeCapacity)
{
    auto effect                                 = MakeEffect();
    effect.emitters.front().spawn.rate          = 1e30f;
    effect.emitters.front().spawn.burstInterval = 1e-30f;
    effect.emitters.front().spawn.burstCount    = 1000000u;
    ParticleSolver solver;
    ASSERT_TRUE(solver.Init(CompileParticleEffect(effect).programs.front()));
    solver.Step(.01f);
    EXPECT_EQ(solver.GetStorage().aliveCount, effect.emitters.front().capacity);
}

TEST(ParticleCompiler, RejectsUnsupportedTargetsAndGridOutsideTheSharedLimit)
{
    auto effect                       = MakeEffect();
    effect.emitters.front().simTarget = ParticleSimTarget::GPU;
    EXPECT_FALSE(CompileParticleEffect(effect).succeeded);
    effect.emitters.front().simTarget = ParticleSimTarget::CPU;
    ParticleModuleDesc subUv;
    subUv.type = ParticleModuleType::SUB_UV;
    subUv.parameters.push_back({Name("columns"), ParticleValue{.constant = 65535.f}});
    subUv.parameters.push_back({Name("rows"), ParticleValue{.constant = 65535.f}});
    effect.emitters.front().modules.push_back(subUv);
    EXPECT_TRUE(CompileParticleEffect(effect).succeeded);
    effect.emitters.front().modules.back().parameters.front().value.constant = 65536.f;
    EXPECT_FALSE(CompileParticleEffect(effect).succeeded);
}

TEST(ParticleSolver, InvalidProgramDoesNotReplaceAnInitializedSolver)
{
    const auto compiled = CompileParticleEffect(MakeEffect());
    ASSERT_TRUE(compiled.succeeded);
    ParticleSolver solver;
    ASSERT_TRUE(solver.Init(compiled.programs.front()));
    solver.Step(.1f, 2);
    const auto count                   = solver.GetStorage().aliveCount;
    const auto hash                    = solver.GetProgram().hash;
    auto       invalid                 = compiled.programs.front();
    invalid.updateModules.front().type = ParticleModuleType::END;
    EXPECT_FALSE(solver.Init(invalid));
    EXPECT_EQ(solver.GetStorage().aliveCount, count);
    EXPECT_EQ(solver.GetProgram().hash, hash);
    invalid               = compiled.programs.front();
    invalid.layout.stride = 0;
    EXPECT_FALSE(solver.Init(invalid));
    EXPECT_EQ(solver.GetStorage().aliveCount, count);
}
