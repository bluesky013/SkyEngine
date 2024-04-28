//
// Created by Zach Lee on 2023/9/16.
//

#pragma once

#include <core/math/Vector3.h>
#include <vector>

namespace sky {

    // components
    struct ParticleLifeTime {
        float duration = 1.f;
        float lifeTime = 1.f;
    };

    struct ParticlePosition {
        Vector3 pos;
        Vector3 acc;
        Vector3 vel;
    };

    struct ParticleRotation {
        Vector3 axis;
        Vector3 speed;
    };

    // storage
    struct ParticleStorage {
        using Storage = std::vector<uint8_t>;

        template <typename ...T, typename Func>
        void ForEach(uint32_t begin, uint32_t end, Func &&func)
        {
            for (uint32_t i = begin; i < end; ++i) {
                func(Component<T>(i)...);
            }
        }

        template <typename T>
        inline T &Component(uint32_t index)
        {
            const auto &[offset, stride] = perParticleIndexMap.at(RuntimeTypeId<T>());
            T *val = reinterpret_cast<T*>(storages[offset].data());
            return val[entities[index]];
        }

        uint32_t currentIndex = 0;
        // entities
        std::vector<uint32_t> entities;

        // per particle storages
        std::vector<Storage> storages;
        std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> perParticleIndexMap;
    };

    // modules
    struct ParticleSpawnParam {
        float delta;
        float time;
    };

    struct ParticleModuleParam {
        float delta;
        float time;
        uint32_t begin = 0;
        uint32_t end = 0;
    };

    struct ParticleSpawnSimple {
        float speed;

        static uint32_t Execute(ParticleSpawnSimple &spawn, const ParticleSpawnParam &info)
        {
            return static_cast<uint32_t>(info.delta * spawn.speed);
        }
    };

    struct ParticleInitAlignedBox {
        Vector3 min;
        Vector3 max;

        static void Execute(ParticleInitAlignedBox &box, ParticleStorage &storage, const ParticleModuleParam &info)
        {
            storage.ForEach<ParticlePosition>(info.begin, info.end, [](ParticlePosition &pos) {

            });
        }
    };

    struct ParticlePositionSolver {
        static void Execute(ParticlePositionSolver &solver, ParticleStorage &storage, const ParticleModuleParam &info)
        {
            storage.ForEach<ParticlePosition>(info.begin, info.end, [](ParticlePosition &pos) {

            });
        }
    };
} // namespace sky