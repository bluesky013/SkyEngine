#include <particle/ParticleSolver.h>

#include <algorithm>
#include <cmath>
#include <core/async/Task.h>

namespace sky::particle {
    namespace {
        uint32_t NextRandom(uint64_t &state)
        {
            constexpr uint64_t MULTIPLIER = 6364136223846793005ull;
            constexpr uint64_t INCREMENT  = 1442695040888963407ull;
            state                         = state * MULTIPLIER + INCREMENT;
            const uint32_t xorshifted     = static_cast<uint32_t>(((state >> 18u) ^ state) >> 27u);
            const uint32_t rotation       = static_cast<uint32_t>(state >> 59u);
            return (xorshifted >> rotation) | (xorshifted << ((32u - rotation) & 31u));
        }

        float UnitFloat(uint32_t value)
        {
            return float(value & 0x00ffffffu) / float(0x01000000u);
        }

        const ParticleCompiledModule *FindModule(const std::vector<ParticleCompiledModule> &modules, ParticleModuleType type)
        {
            const auto found = std::find_if(modules.begin(), modules.end(), [&](const auto &module) { return module.type == type; });
            return found == modules.end() ? nullptr : &*found;
        }
    } // namespace

    bool ParticleSolver::Init(const ParticleProgram &inProgram, ParticleSolverDesc inDesc, const Matrix4 &inSpawnTransform)
    {
        std::string error;
        if (!ValidateParticleProgram(inProgram, error) || !std::isfinite(inDesc.rateScale) || inDesc.rateScale < 0.f ||
            !std::isfinite(inDesc.gravity) || !std::isfinite(inDesc.drag) || inDesc.drag < 0.f ||
            !std::all_of(std::begin(inSpawnTransform.v), std::end(inSpawnTransform.v), [](float value) { return std::isfinite(value); }))
            return false;
        program        = inProgram;
        desc           = inDesc;
        rateScale      = desc.rateScale;
        spawnTransform = inSpawnTransform;
        if (!storage.Init(program.capacity))
            return false;
        randomState      = desc.randomSeed;
        spawnAccumulator = 0.f;
        burstAccumulator = 0.f;
        events.clear();
        events.reserve(8192);
        for (float time = 0.f; time < program.spawn.warmupSeconds; time += 1.f / 30.f)
            Step(std::min(1.f / 30.f, program.spawn.warmupSeconds - time));
        return true;
    }

    void ParticleSolver::Reset()
    {
        storage.Reset();
        randomState      = desc.randomSeed;
        spawnAccumulator = 0.f;
        burstAccumulator = 0.f;
        events.clear();
    }

    void ParticleSolver::SpawnOne(uint32_t seed)
    {
        float    lifetime = 1.f, size = 1.f, rotation = 0.f;
        Vector3  position{}, velocity{};
        uint64_t sampleState = seed;
        for (const auto &module : program.spawnModules) {
            if (module.type != ParticleModuleType::INITIALIZE)
                continue;
            float values[3];
            for (uint32_t i = 0; i < 3; ++i) {
                values[i] = module.values[i];
                if (module.randomMask & (1u << i))
                    values[i] =
                        static_cast<float>(double(values[i]) + (double(module.randomMaximum[i]) - values[i]) * UnitFloat(NextRandom(sampleState)));
            }
            lifetime = values[0];
            size     = values[1];
            rotation = values[2];
            position = module.position;
            velocity = module.velocity;
        }
        if (!storage.Spawn(lifetime > 0.f ? lifetime : 1.f, seed))
            return;
        const auto slot = storage.alive.back();
        if (program.space == ParticleSimSpace::WORLD) {
            const auto p = spawnTransform * Vector4{position.x, position.y, position.z, 1.f};
            const auto v = spawnTransform * Vector4{velocity.x, velocity.y, velocity.z, 0.f};
            position     = {p.x, p.y, p.z};
            velocity     = {v.x, v.y, v.z};
        }
        storage.positions[slot]  = position;
        storage.velocities[slot] = velocity;
        if (events.size() < 8192)
            events.push_back({ParticleEventType::SPAWN, position, seed});
        storage.sizes[slot]     = size;
        storage.rotations[slot] = rotation;
        if (const auto *color = FindModule(program.updateModules, ParticleModuleType::COLOR_OVER_LIFE))
            storage.colors[slot] = color->gradient.Evaluate(0.f);
    }

    void ParticleSolver::Step(float deltaSeconds, uint32_t spawnedThisFrame)
    {
        if (!(deltaSeconds > 0.f) || !std::isfinite(deltaSeconds))
            return;
        const double accumulated = double(spawnAccumulator) + double(program.spawn.rate) * rateScale * deltaSeconds;
        const double whole       = std::floor(accumulated);
        spawnAccumulator         = static_cast<float>(accumulated - whole);
        double requested         = double(spawnedThisFrame) + whole;
        if (program.spawn.burstInterval > 0.f && program.spawn.burstCount > 0) {
            const double elapsed = double(burstAccumulator) + deltaSeconds;
            const double bursts  = std::floor(elapsed / program.spawn.burstInterval);
            burstAccumulator     = static_cast<float>(std::fmod(elapsed, program.spawn.burstInterval));
            requested += bursts * program.spawn.burstCount * rateScale;
        }
        const auto spawnCount = static_cast<uint32_t>(std::min(requested, double(storage.freeList.size())));
        for (uint32_t i = 0; i < spawnCount; ++i)
            SpawnOne(NextRandom(randomState));
        UpdateParticles(deltaSeconds);
        KillExpired();
    }

    void ParticleSolver::UpdateParticles(float deltaSeconds)
    {
        const bool explicitGravity = FindModule(program.updateModules, ParticleModuleType::GRAVITY) != nullptr;
        const bool explicitDrag    = FindModule(program.updateModules, ParticleModuleType::DRAG) != nullptr;
        const auto update          = [&](size_t begin, size_t end) {
            for (size_t index = begin; index < end; ++index) {
                const auto slot = storage.alive[index];
                storage.ages[slot] += deltaSeconds;
                const auto normalized = std::clamp(storage.ages[slot] / storage.lifetimes[slot], 0.f, 1.f);
                if (!explicitGravity)
                    storage.velocities[slot].y += desc.gravity * deltaSeconds;
                if (!explicitDrag && desc.drag > 0.f)
                    storage.velocities[slot] = storage.velocities[slot] * std::max(0.f, 1.f - desc.drag * deltaSeconds);
                storage.rotations[slot] += storage.angularVelocities[slot] * deltaSeconds;
                for (const auto &module : program.updateModules) {
                    switch (module.type) {
                    case ParticleModuleType::GRAVITY:
                        storage.velocities[slot] += Vector3{module.values[0], module.values[1], module.values[2]} * deltaSeconds;
                        break;
                    case ParticleModuleType::DRAG:
                        storage.velocities[slot] = storage.velocities[slot] * std::max(0.f, 1.f - module.values[0] * deltaSeconds);
                        break;
                    case ParticleModuleType::COLOR_OVER_LIFE: storage.colors[slot] = module.gradient.Evaluate(normalized); break;
                    case ParticleModuleType::SIZE_OVER_LIFE: storage.sizes[slot] = module.curve.Evaluate(normalized).x; break;
                    case ParticleModuleType::ROTATION_OVER_LIFE: storage.rotations[slot] = module.curve.Evaluate(normalized).x; break;
                    case ParticleModuleType::SUB_UV: storage.subUv[slot] = std::max(0.f, module.curve.Evaluate(normalized).x); break;
                    case ParticleModuleType::KILL_VOLUME:
                        if (storage.positions[slot].y >= module.values[0] && storage.positions[slot].y <= module.values[1])
                            storage.ages[slot] = storage.lifetimes[slot];
                        break;
                    default: break;
                    }
                }
                storage.positions[slot] += storage.velocities[slot] * deltaSeconds;
            }
        };
        auto      &pool   = TaskExecutor::Get()->GetPool();
        const auto count  = storage.alive.size();
        const auto chunks = count / 256;
        if (chunks < 2 || pool.GetThreadCount() < 2) {
            update(0, count);
        } else {
            const auto taskCount = static_cast<uint32_t>(std::min<size_t>(chunks, pool.GetThreadCount()));
            auto       future    = pool.Parallel(
                taskCount, [&](uint32_t index, uint32_t total, ThreadContext &) { update(count * index / total, count * (index + 1) / total); });
            future.wait();
        }
    }

    void ParticleSolver::KillExcess(uint32_t maximum)
    {
        storage.KillExcess(maximum);
    }

    void ParticleSolver::SetRateScale(float value)
    {
        rateScale = std::isfinite(value) ? std::max(0.f, value) : 0.f;
    }

    void ParticleSolver::SetSpawnTransform(const Matrix4 &value)
    {
        spawnTransform = value;
    }

    void ParticleSolver::DrainEvents(std::vector<ParticleEvent> &output)
    {
        output.insert(output.end(), events.begin(), events.end());
        events.clear();
    }

    std::vector<ParticleEvent> ParticleSolver::DrainEvents()
    {
        std::vector<ParticleEvent> output;
        DrainEvents(output);
        return output;
    }

    void ParticleSolver::KillExpired()
    {
        for (size_t i = storage.alive.size(); i-- > 0;) {
            const auto slot = storage.alive[i];
            if (storage.ages[slot] >= storage.lifetimes[slot]) {
                if (events.size() < 8192)
                    events.push_back({ParticleEventType::DEATH, storage.positions[slot], storage.seeds[slot]});
                storage.KillAliveIndex(i);
            }
        }
    }
} // namespace sky::particle
