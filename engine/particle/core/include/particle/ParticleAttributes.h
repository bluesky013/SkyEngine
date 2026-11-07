#pragma once

#include <array>
#include <cstdint>

namespace sky::particle {

    enum class ParticleAttribute : uint8_t {
        POSITION,
        VELOCITY,
        AGE,
        LIFETIME,
        COLOR,
        SIZE,
        ROTATION,
        ANGULAR_VELOCITY,
        SPRITE_INDEX,
        SUB_UV,
        SEED,
        CUSTOM0,
        CUSTOM1,
        CUSTOM2,
        CUSTOM3,
        END
    };

    constexpr uint32_t PARTICLE_ATTRIBUTE_COUNT = static_cast<uint32_t>(ParticleAttribute::END);

    struct ParticleAttributeLayout {
        uint32_t                                       version = 1;
        uint32_t                                       stride  = 0;
        std::array<uint32_t, PARTICLE_ATTRIBUTE_COUNT> offsets{};
    };

    bool IsValidAttribute(ParticleAttribute attribute) noexcept;

} // namespace sky::particle
