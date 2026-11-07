#pragma once

#include <particle/ParticleAttributes.h>
#include <particle/ParticleEffect.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sky::particle {

    enum class ParticleProgramFlags : uint32_t {
        NONE               = 0,
        GRAVITY            = 1u << 0,
        DRAG               = 1u << 1,
        COLOR_OVER_LIFE    = 1u << 2,
        SIZE_OVER_LIFE     = 1u << 3,
        ROTATION_OVER_LIFE = 1u << 4,
        SUB_UV             = 1u << 5,
        KILL_VOLUME        = 1u << 6,
        BURST              = 1u << 7,
    };

    constexpr ParticleProgramFlags operator|(ParticleProgramFlags a, ParticleProgramFlags b) noexcept
    {
        return static_cast<ParticleProgramFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    struct ParticleCompiledModule {
        ParticleModuleType   type = ParticleModuleType::INITIALIZE;
        std::array<float, 4> values{};
        ParticleAttribute    attribute = ParticleAttribute::SIZE;
        ParticleCurve        curve;
        ParticleGradient     gradient;
        uint32_t             randomMask = 0;
        std::array<float, 3> randomMaximum{};
        Vector3              position{};
        Vector3              velocity{};
    };

    struct ParticleCompiledRenderer {
        ParticleRendererType type  = ParticleRendererType::SPRITE;
        ParticleBlendMode    blend = ParticleBlendMode::ALPHA;
        bool                 lit   = false;
        bool                 soft  = false;
        Uuid                 texture;
    };

    inline uint32_t RendererVariantKey(const ParticleCompiledRenderer &renderer) noexcept
    {
        uint32_t   key = 2166136261u;
        const auto mix = [&](uint32_t value) { key = (key ^ value) * 16777619u; };
        mix(static_cast<uint32_t>(renderer.type));
        mix(static_cast<uint32_t>(renderer.blend));
        mix(renderer.lit ? 1u : 0u);
        mix(renderer.soft ? 2u : 0u);
        mix(renderer.texture ? 4u : 0u);
        return key;
    }

    struct ParticleProgram {
        uint64_t                              hash = 0;
        Name                                  emitter;
        ParticleProgramFlags                  flags = ParticleProgramFlags::NONE;
        ParticleAttributeLayout               layout;
        std::vector<ParticleCompiledModule>   spawnModules;
        std::vector<ParticleCompiledModule>   updateModules;
        std::vector<ParticleCompiledRenderer> renderers;
        ParticleSpawnDesc                     spawn;
        uint32_t                              capacity = 0;
        ParticleBounds                        bounds;
        ParticleSimSpace                      space = ParticleSimSpace::WORLD;
    };

    struct ParticleCompileDiagnostic {
        uint32_t    emitter = 0;
        std::string message;
    };

    struct ParticleCompileResult {
        bool                                   succeeded = false;
        std::vector<ParticleProgram>           programs;
        std::vector<ParticleCompileDiagnostic> diagnostics;
    };

    bool                  ValidateParticleProgram(const ParticleProgram &program, std::string &error);
    uint64_t              HashParticleProgram(const ParticleProgram &program);
    ParticleCompileResult CompileParticleEffect(const ParticleEffectDesc &effect);

} // namespace sky::particle
