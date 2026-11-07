#pragma once

#include <particle/ParticleEffect.h>

#include <span>
#include <string_view>

namespace sky::particle {
    inline constexpr uint32_t MAX_PARTICLE_CAPACITY         = 16u * 1024u * 1024u;
    inline constexpr uint32_t MAX_PARTICLE_EMITTERS         = 64;
    inline constexpr uint32_t MAX_PARTICLE_MODULES          = 256;
    inline constexpr uint32_t MAX_PARTICLE_KEYS             = 4096;
    inline constexpr uint32_t MAX_PARTICLE_RENDERERS        = 16;
    inline constexpr uint32_t MAX_PARTICLE_SUB_UV_DIMENSION = 65535;
    inline constexpr float    MAX_PARTICLE_WARMUP_SECONDS   = 60.f;

    enum class ParticleParameterShape { SCALAR, VECTOR, GRADIENT };
    struct ParticleParameterSchema {
        std::string_view       name;
        ParticleParameterShape shape;
        uint32_t               kinds;
    };
    struct ParticleModuleSchema {
        ParticleModuleType                       type;
        std::string_view                         token;
        std::string_view                         label;
        std::span<const ParticleParameterSchema> parameters;
    };
    std::span<const ParticleModuleSchema> ParticleModuleSchemas();
    const ParticleModuleSchema           *FindParticleModuleSchema(ParticleModuleType type);
    ParticleModuleType                    ParseParticleModuleType(std::string_view token);
    bool                                  ValidateParticleValue(const ParticleValue &value, std::string &error);
    bool                                  ValidateParticleEffect(const ParticleEffectDesc &effect, std::string &error);
} // namespace sky::particle
