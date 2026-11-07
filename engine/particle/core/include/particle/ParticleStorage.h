#pragma once

#include <core/math/Color.h>
#include <core/math/Vector3.h>
#include <particle/ParticleProgram.h>

#include <cstdint>
#include <vector>

namespace sky::particle {

    // Fixed-capacity SoA storage; slots are reused through the free list.
    struct ParticleStorage {
        uint32_t              capacity   = 0;
        uint32_t              aliveCount = 0;
        std::vector<Vector3>  positions;
        std::vector<Vector3>  velocities;
        std::vector<Color>    colors;
        std::vector<float>    ages;
        std::vector<float>    lifetimes;
        std::vector<float>    sizes;
        std::vector<float>    rotations;
        std::vector<float>    angularVelocities;
        std::vector<uint32_t> spriteIndices;
        std::vector<float>    subUv;
        std::vector<uint32_t> seeds;
        std::vector<uint32_t> alive;
        std::vector<uint32_t> freeList;
        std::vector<uint8_t>  active;

        bool Init(uint32_t inCapacity);
        void Reset();
        bool Spawn(float lifetime, uint32_t seed);
        void Kill(uint32_t slot);
        void KillAliveIndex(size_t index);
        bool Alive(uint32_t slot) const;
        void KillExcess(uint32_t maximum);
    };

} // namespace sky::particle
