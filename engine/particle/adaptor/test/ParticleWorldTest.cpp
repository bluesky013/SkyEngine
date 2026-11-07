#include <framework/world/World.h>
#include <gtest/gtest.h>
#include <limits>
#include <particle/ParticleEffect.h>
#include <particle/ParticleSchema.h>
#include <particle/assets/ParticleEffectAsset.h>
#include <particle/assets/ParticlePreviewEffectRegistry.h>
#include <particle/world/ParticleComponent.h>
#include <particle/world/ParticleSystem.h>

using namespace sky;
using namespace sky::particle;

TEST(ParticleWorld, InMemoryPreviewEffectDrivesSimulationWithoutACookedProduct)
{
    ParticleEffectDesc                 effect;
    effect.fixedBounds                 = {{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}};
    ParticleEmitterDesc                emitter;
    emitter.name                       = Name("spark");
    emitter.capacity                   = 32;
    emitter.spawn.rate                 = 40.f;
    emitter.bounds                     = effect.fixedBounds;
    emitter.modules.push_back({ParticleModuleType::INITIALIZE, Name("Initialize"), {}});
    emitter.renderers.push_back({});
    effect.emitters.push_back(emitter);
    const auto compiled = CompileParticleEffect(effect);
    ASSERT_TRUE(compiled.succeeded);

    auto data       = std::make_shared<ParticleEffectAssetData>();
    data->programs  = compiled.programs;
    data->valid     = true;

    CounterPtr<World> world(World::CreateWorld());
    ASSERT_TRUE(world);
    auto *system = new ParticleSystem();
    world->AddSubSystem(Name(ParticleSystem::NAME.data()), system);
    auto *actor = world->CreateActor();
    ASSERT_TRUE(actor);
    auto *component = actor->AddComponent<ParticleComponent>();
    ASSERT_TRUE(component);
    const Uuid effectId = Uuid::Create();
    component->SetEffect(effectId);
    ParticlePreviewEffectRegistry::Get()->Set(world->GetRuntimeId(), effectId, data);
    world->StartSimulation();
    world->Tick(0.f);
    world->Tick(0.1f);
    EXPECT_GT(system->GetActiveParticleCount(), 0u);

    ParticlePreviewEffectRegistry::Get()->Clear(world->GetRuntimeId(), effectId);
    world->Tick(0.1f);
    EXPECT_EQ(system->GetActiveParticleCount(), 0u);
    world.Reset(nullptr);
}

TEST(ParticleWorld, ComponentLifecycleAndDetachAreStableWithoutAnEffect)
{
    CounterPtr<World> world(World::CreateWorld());
    ASSERT_TRUE(world);
    auto *system = new ParticleSystem();
    world->AddSubSystem(Name(ParticleSystem::NAME.data()), system);
    auto *actor = world->CreateActor();
    ASSERT_TRUE(actor);
    auto *component = actor->AddComponent<ParticleComponent>();
    ASSERT_TRUE(component);
    EXPECT_TRUE(component->GetAttachment());
    component->SetEffect(Uuid::Create());
    world->StartSimulation();
    world->Tick(0.1f);
    EXPECT_EQ(system->GetActiveParticleCount(), 0u);
    actor->RemoveComponent<ParticleComponent>();
    world->Tick(0.1f);
    EXPECT_EQ(system->GetActiveParticleCount(), 0u);
    world.Reset(nullptr);
}

TEST(ParticleWorld, ConfigValidationRangeIsExplicit)
{
    ParticleWorldConfig config;
    std::string         error;
    config.maxParticles = 0;
    EXPECT_FALSE(ValidateParticleWorldConfig(config, error));
    config.maxParticles = 16u * 1024u * 1024u + 1;
    EXPECT_FALSE(ValidateParticleWorldConfig(config, error));
    config.maxParticles = 1024;
    EXPECT_TRUE(ValidateParticleWorldConfig(config, error));
    EXPECT_TRUE(error.empty());
    config.lodMinRate = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(ValidateParticleWorldConfig(config, error));
    config.lodMinRate = .5f;
    config.lodEnd     = config.lodStart - 1.f;
    EXPECT_FALSE(ValidateParticleWorldConfig(config, error));
}


