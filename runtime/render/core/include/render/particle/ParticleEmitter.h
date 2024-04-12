//
// Created by Zach Lee on 2023/9/16.
//

#pragma once

#include <core/type/Type.h>
#include <render/particle/ParticleModules.h>
#include <render/particle/ParticleEmitter.h>
#include <vector>
#include <list>
#include <unordered_map>

namespace sky {
    struct ParticleSpawnModule {
        using Func = uint32_t(*)(void *, const ParticleSpawnParam &info);
        Func fn = nullptr;
        uint32_t offset = 0;
    };

    struct ParticleModule {
        using Func = void(*)(void *ptr, ParticleStorage &storage, const ParticleModuleParam &info);
        Func fn = nullptr;
        uint32_t offset = 0;
    };

    class ParticleEmitter {
    public:
        ParticleEmitter() = default;
        ~ParticleEmitter() = default;

        void Tick(float delta, float time);
        void SetCapacity(uint32_t size);
        void SetTimeRange(float start, float end);

        template <typename T>
        void RegisterPerParticleComponent()
        {
            auto id = TypeInfo<T>::Hash();
            auto iter = particleStorage.perParticleIndexMap.find(id);
            if (iter != particleStorage.perParticleIndexMap.end()) {
                particleStorage.perParticleIndexMap.emplace(id, std::pair{static_cast<uint32_t>(particleStorage.storages.size()), static_cast<uint32_t>(sizeof(T))});
                particleStorage.storages.emplace_back();
            }
        }

        template <typename T>
        uint32_t RegisterModule(const T &val)
        {
            auto offset = static_cast<uint32_t>(moduleStorage.size());
            moduleStorage.resize(offset + sizeof(T));
            new (&moduleStorage[offset]) T(val);
            return offset;
        }


        template <typename T>
        static uint32_t SpawnExecute(void *ptr, const ParticleSpawnParam &info)
        {
            return T::Execute(*reinterpret_cast<T*>(ptr), info);
        }

        template <typename T>
        static void ModuleExecute(void *ptr, ParticleStorage &storage, const ParticleModuleParam &param)
        {
            return T::Execute(*reinterpret_cast<T*>(ptr), storage, param);
        }

        template <typename T>
        void AddSpawnModule(const T &data)
        {
            spawnModules.emplace_back(ParticleSpawnModule{&ParticleEmitter::SpawnExecute<T>, RegisterModule(data)});
        }

        template <typename T>
        void AddInitModule(const T &data)
        {
            initModules.emplace_back(ParticleModule{&ParticleEmitter::ModuleExecute<T>, RegisterModule(data)});
        }

        template <typename T>
        void AddSolverModule(const T &data)
        {
            solverModules.emplace_back(ParticleModule{&ParticleEmitter::ModuleExecute<T>, RegisterModule(data)});
        }

        using Storage = std::vector<uint8_t>;
    private:
        float timeStart = 0.f;
        float timeEnd = 0.f;

        // per particle storage
        ParticleStorage particleStorage;

        // per instance storage
        Storage moduleStorage;

        // spawn
        std::list<ParticleSpawnModule> spawnModules;

        // init
        std::list<ParticleModule> initModules;

        // solvers
        std::list<ParticleModule> solverModules;

        // renderer
    };

} // namespace sky