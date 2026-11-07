#include <particle/render/ParticleSpritePacking.h>

namespace sky::particle {
    ParticleSpriteInstance PackParticleSpriteInstance(const ParticleRenderInstance &particle)
    {
        return {{particle.position.x, particle.position.y, particle.position.z, particle.size},
                particle.color,
                {particle.rotation, 0.f, 0.f, 0.f},
                {particle.subU, particle.subV, particle.subWidth, particle.subHeight}};
    }
} // namespace sky::particle
