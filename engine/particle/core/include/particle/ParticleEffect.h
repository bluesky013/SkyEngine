#pragma once

#include <core/math/Color.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/name/Name.h>
#include <core/util/Uuid.h>
#include <particle/ParticleAttributes.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::particle {

    enum class ParticleSimTarget : uint8_t { CPU, GPU, END };
    enum class ParticleSimSpace : uint8_t { WORLD, LOCAL, CUSTOM, END };
    enum class ParticleRendererType : uint8_t { SPRITE, MESH, END };
    enum class ParticleBlendMode : uint8_t { ADDITIVE, ALPHA, PREMULTIPLIED, END };

    enum class ParticleModuleType : uint8_t {
        SPAWN_RATE,
        BURST,
        INITIALIZE,
        GRAVITY,
        DRAG,
        COLOR_OVER_LIFE,
        SIZE_OVER_LIFE,
        ROTATION_OVER_LIFE,
        SUB_UV,
        KILL_VOLUME,
        END
    };

    struct ParticleRange {
        float min = 0.f;
        float max = 0.f;
    };

    struct ParticleCurveKey {
        float   time = 0.f;
        Vector3 value{1.f, 1.f, 1.f};
    };

    struct ParticleGradientKey {
        float time = 0.f;
        Color color{1.f, 1.f, 1.f, 1.f};
    };

    struct ParticleCurve {
        std::vector<ParticleCurveKey> keys;
        Vector3                       Evaluate(float time) const;
    };

    struct ParticleGradient {
        std::vector<ParticleGradientKey> keys;
        Color                            Evaluate(float time) const;
    };

    enum class ParticleValueKind : uint8_t { CONSTANT, RANDOM, CURVE, GRADIENT, PARAMETER, ATTRIBUTE, END };

    struct ParticleValue {
        ParticleValueKind kind     = ParticleValueKind::CONSTANT;
        float             constant = 0.f;
        Vector3           vector{0.f, 0.f, 0.f};
        ParticleRange     range;
        ParticleCurve     curve;
        ParticleGradient  gradient;
        Name              parameter;
        ParticleAttribute attribute = ParticleAttribute::SIZE;
    };

    struct ParticleParameterDesc {
        Name          name;
        ParticleValue value;
    };

    struct ParticleModuleDesc {
        ParticleModuleType                 type = ParticleModuleType::INITIALIZE;
        Name                               name;
        std::vector<ParticleParameterDesc> parameters;
    };

    struct ParticleSpawnDesc {
        float    rate          = 0.f;
        float    burstInterval = 0.f;
        uint32_t burstCount    = 0;
        float    warmupSeconds = 0.f;
    };

    struct ParticleBounds {
        Vector3 min{0.f, 0.f, 0.f};
        Vector3 max{0.f, 0.f, 0.f};
        bool    Valid() const;
    };

    struct ParticleRendererDesc {
        ParticleRendererType type  = ParticleRendererType::SPRITE;
        ParticleBlendMode    blend = ParticleBlendMode::ALPHA;
        bool                 lit   = false;
        bool                 soft  = false;
        Uuid                 texture;
    };

    struct ParticleEmitterDesc {
        Name                              name;
        ParticleSimTarget                 simTarget = ParticleSimTarget::CPU;
        ParticleSimSpace                  space     = ParticleSimSpace::WORLD;
        uint32_t                          capacity  = 0;
        ParticleSpawnDesc                 spawn;
        ParticleBounds                    bounds;
        std::vector<ParticleModuleDesc>   modules;
        std::vector<ParticleRendererDesc> renderers;
    };

    struct ParticleUserParameter {
        Name          name;
        ParticleValue value;
    };

    struct ParticleEffectDesc {
        uint32_t                           version = 1;
        std::vector<ParticleUserParameter> userParameters;
        std::vector<ParticleEmitterDesc>   emitters;
        ParticleBounds                     fixedBounds;
    };

    bool IsValidModuleType(ParticleModuleType type) noexcept;
    bool IsValidSimTarget(ParticleSimTarget target) noexcept;
    bool IsValidRendererType(ParticleRendererType type) noexcept;

} // namespace sky::particle
