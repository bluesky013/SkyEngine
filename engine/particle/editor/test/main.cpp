#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <particle/assets/ParticleEffectAsset.h>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    sky::particle::RegisterParticleAssets(sky::SerializationContext::Get());
    sky::particle::RegisterParticleAuthoringTypes(sky::SerializationContext::Get());
    return RUN_ALL_TESTS();
}
