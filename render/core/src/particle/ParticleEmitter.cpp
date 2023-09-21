//
// Created by Zach Lee on 2023/9/16.
//

#include <render/particle/ParticleEmitter.h>

namespace sky {
    void ParticleEmitter::Tick(float delta, float time)
    {
        // spawn
        uint32_t spawnNum = 0;
        ParticleSpawnParam spInfo = {delta, time};
        for (auto &s : spawnModules) {
            spawnNum += s.fn(moduleStorage.data() + s.offset, spInfo);
        }
        spawnNum = std::min(spawnNum, static_cast<uint32_t>(particleStorage.entities.size() - particleStorage.currentIndex));

        ParticleModuleParam siInfo = {delta, time, particleStorage.currentIndex, particleStorage.currentIndex + spawnNum};
        // init
        for (auto &i : initModules) {
            i.fn(moduleStorage.data() + i.offset, particleStorage, siInfo);
        }

        // solvers
        for (auto &s : solverModules) {
            s.fn(moduleStorage.data() + s.offset, particleStorage, siInfo);
        }

        // recycle
    }

    void ParticleEmitter::SetTimeRange(float start, float end)
    {
        timeStart = start;
        timeEnd = end;
    }

    void ParticleEmitter::SetCapacity(uint32_t size)
    {
        particleStorage.entities.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            particleStorage.entities[i] = i;
        }

        for (auto pair : particleStorage.perParticleIndexMap) {
            auto &[index, stride] = pair.second;
            particleStorage.storages[index].resize(size * stride);
        }

        particleStorage.currentIndex = 0;
    }

} // namespace sky