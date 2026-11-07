#pragma once

#include <aurora/shader/SharedShader.h>

#define AURORA_PARTICLE_SPRITE_FIELDS(F, A, T) F(T, MAT4, viewProjection) F(T, FLOAT4, cameraRight) F(T, FLOAT4, cameraUp)

#define AURORA_PARTICLE_SPRITE_RESOURCES(R) \
    R(UNIFORM, ParticleSpriteParams, gParticleSprite, 1, 0, AURORA_PARTICLE_SPRITE_FIELDS) \
    R(TEXTURE, TEXTURE2D, FLOAT4, gSpriteTexture, 1, 1) \
    R(SAMPLER, REGULAR, gSpriteSampler, 1, 2)

AURORA_SHADER_PASS(sky::particle::shared, ParticleSpritePass, AURORA_PARTICLE_SPRITE_RESOURCES)

#undef AURORA_PARTICLE_SPRITE_RESOURCES
#undef AURORA_PARTICLE_SPRITE_FIELDS
