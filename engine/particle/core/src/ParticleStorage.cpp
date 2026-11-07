#include <particle/ParticleStorage.h>

#include <particle/ParticleSchema.h>

#include <algorithm>

namespace sky::particle {
    bool ParticleStorage::Init(uint32_t inCapacity)
    {
        if (!inCapacity || inCapacity > MAX_PARTICLE_CAPACITY)
            return false;
        capacity = inCapacity;
        positions.assign(capacity, Vector3{});
        velocities.assign(capacity, Vector3{});
        colors.assign(capacity, Color{1.f, 1.f, 1.f, 1.f});
        ages.assign(capacity, 0.f);
        lifetimes.assign(capacity, 1.f);
        sizes.assign(capacity, 1.f);
        rotations.assign(capacity, 0.f);
        angularVelocities.assign(capacity, 0.f);
        spriteIndices.assign(capacity, 0u);
        subUv.assign(capacity, 0.f);
        seeds.assign(capacity, 0u);
        active.assign(capacity, 0u);
        alive.clear();
        alive.reserve(capacity);
        freeList.resize(capacity);
        for (uint32_t i = 0; i < capacity; ++i)
            freeList[capacity - 1 - i] = i;
        aliveCount = 0;
        return true;
    }

    void ParticleStorage::Reset()
    {
        std::fill(active.begin(), active.end(), uint8_t{0});
        alive.clear();
        freeList.clear();
        freeList.reserve(capacity);
        for (uint32_t i = 0; i < capacity; ++i)
            freeList.push_back(capacity - 1 - i);
        aliveCount = 0;
    }

    bool ParticleStorage::Spawn(float lifetime, uint32_t seed)
    {
        if (freeList.empty())
            return false;
        const auto slot = freeList.back();
        freeList.pop_back();
        active[slot]            = 1;
        ages[slot]              = 0.f;
        lifetimes[slot]         = lifetime;
        seeds[slot]             = seed;
        positions[slot]         = {};
        velocities[slot]        = {};
        colors[slot]            = Color{1.f, 1.f, 1.f, 1.f};
        sizes[slot]             = 1.f;
        rotations[slot]         = 0.f;
        angularVelocities[slot] = 0.f;
        spriteIndices[slot]     = 0u;
        subUv[slot]             = 0.f;
        alive.push_back(slot);
        ++aliveCount;
        return true;
    }

    void ParticleStorage::Kill(uint32_t slot)
    {
        if (slot >= capacity || !active[slot])
            return;
        const auto found = std::find(alive.begin(), alive.end(), slot);
        if (found != alive.end())
            KillAliveIndex(static_cast<size_t>(found - alive.begin()));
    }

    void ParticleStorage::KillAliveIndex(size_t index)
    {
        if (index >= alive.size())
            return;
        const auto slot = alive[index];
        active[slot]    = 0;
        alive[index]    = alive.back();
        alive.pop_back();
        freeList.push_back(slot);
        --aliveCount;
    }

    bool ParticleStorage::Alive(uint32_t slot) const
    {
        return slot < capacity && active[slot] != 0;
    }

    void ParticleStorage::KillExcess(uint32_t maximum)
    {
        if (alive.size() <= maximum)
            return;
        const auto excess = alive.size() - maximum;
        for (size_t i = 0; i < excess; ++i) {
            const auto slot = alive[i];
            active[slot]    = 0;
            freeList.push_back(slot);
        }
        alive.erase(alive.begin(), alive.begin() + static_cast<std::ptrdiff_t>(excess));
        aliveCount = static_cast<uint32_t>(alive.size());
    }
} // namespace sky::particle
