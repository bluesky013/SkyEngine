#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <particle/assets/ParticleEffectAsset.h>
#include <framework/world/TransformComponent.h>
#include <particle/world/ParticleComponent.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::particle::ParticleComponent::Reflect(sky::SerializationContext::Get());
    sky::particle::RegisterParticleAssets(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}
