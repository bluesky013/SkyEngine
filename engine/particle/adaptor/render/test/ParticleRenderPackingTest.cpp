#include <gtest/gtest.h>
#include <particle/render/ParticleSpritePacking.h>

using namespace sky;
using namespace sky::particle;

TEST(ParticleSpritePacking, PacksPositionColorRotationAndUv)
{
    ParticleRenderInstance source;
    source.position   = {1.f, 2.f, 3.f};
    source.size       = 4.f;
    source.rotation   = 0.5f;
    source.color      = {0.1f, 0.2f, 0.3f, 0.4f};
    source.subU       = 0.25f;
    source.subV       = 0.5f;
    source.subWidth   = 0.25f;
    source.subHeight  = 0.25f;
    const auto packed = PackParticleSpriteInstance(source);
    EXPECT_EQ(sizeof(ParticleSpriteInstance), 64u);
    EXPECT_FLOAT_EQ(packed.positionSize.x, 1.f);
    EXPECT_FLOAT_EQ(packed.positionSize.w, 4.f);
    EXPECT_FLOAT_EQ(packed.color.a, 0.4f);
    EXPECT_FLOAT_EQ(packed.rotationSubUv.x, 0.5f);
    EXPECT_FLOAT_EQ(packed.uvRect.x, 0.25f);
    EXPECT_FLOAT_EQ(packed.uvRect.w, 0.25f);
}
