#pragma once

#include <core/math/Matrix4.h>
#include <particle/ParticleEvents.h>
#include <particle/ParticleProgram.h>
#include <particle/ParticleStorage.h>

#include <cstdint>

namespace sky::particle {

    struct ParticleSolverDesc {
        float    gravity       = -9.81f;
        float    drag          = 0.f;
        uint32_t randomSeed = 1u;
        float    rateScale  = 1.f;
    };

    // CPU reference solver used by tests, tools and the CPU emitter path.
    class ParticleSolver {
    public:
        bool Init(const ParticleProgram &program, ParticleSolverDesc desc = {}, const Matrix4 &spawnTransform = Matrix4::Identity());
        void Reset();

        // Advances one frame; spawn is rate-accumulated with a deterministic carry.
        void                       Step(float deltaSeconds, uint32_t spawnedThisFrame = 0);
        void                       KillExcess(uint32_t maximum);
        void                       SetRateScale(float value);
        void                       SetSpawnTransform(const Matrix4 &value);
        void                       DrainEvents(std::vector<ParticleEvent> &output);
        std::vector<ParticleEvent> DrainEvents();

        const ParticleStorage &GetStorage() const
        {
            return storage;
        }
        const ParticleProgram &GetProgram() const
        {
            return program;
        }

    private:
        void SpawnOne(uint32_t seed);
        void UpdateParticles(float deltaSeconds);
        void KillExpired();

        ParticleProgram            program;
        ParticleSolverDesc         desc;
        ParticleStorage            storage;
        float                      spawnAccumulator = 0.f;
        float                      rateScale        = 1.f;
        float                      burstAccumulator = 0.f;
        Matrix4                    spawnTransform;
        uint64_t                   randomState = 1u;
        std::vector<ParticleEvent> events;
    };

} // namespace sky::particle
