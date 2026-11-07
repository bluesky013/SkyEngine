#pragma once

#include <particle/ParticleEffect.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace sky::particle {
    inline constexpr size_t MAX_PARTICLE_SOURCE_BYTES = 4u * 1024u * 1024u;

    bool ParseParticleSource(std::string_view text, ParticleEffectDesc &effect, std::string &error);
    bool WriteParticleSource(const ParticleEffectDesc &effect, std::string &text);

} // namespace sky::particle
