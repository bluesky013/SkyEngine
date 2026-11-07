#pragma once

#include <core/math/Vector3.h>

#include <cstdint>
#include <span>

namespace sky::particle {

    enum class ParticleEventType : uint8_t { SPAWN, DEATH, END };

    struct ParticleEvent {
        ParticleEventType type = ParticleEventType::SPAWN;
        Vector3           position{};
        uint32_t          seed = 0;
    };

    class IParticleEventSink {
    public:
        virtual ~IParticleEventSink()                       = default;
        virtual void Submit(std::span<const ParticleEvent>) = 0;
    };

} // namespace sky::particle
