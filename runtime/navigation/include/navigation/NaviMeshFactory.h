//
// Created by blues on 2024/10/11.
//

#pragma once

#include <core/environment/Singleton.h>
#include <navigation/NaviMesh.h>
#include <navigation/NaviMeshGenerator.h>

namespace sky::ai {

    class NaviMeshFactory : public Singleton<NaviMeshFactory> {
    public:
        NaviMeshFactory() = default;
        ~NaviMeshFactory() override = default;

        CounterPtr<NaviMesh> CreateNaviMap();
        CounterPtr<NaviMeshGenerator> CreateGenerator();

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual NaviMesh* CreateNaviMesh() = 0;
            virtual NaviMeshGenerator* CreateGenerator() = 0;
        };

    private:
        std::unique_ptr<Impl> factory;
    };

} // namespace sky::ai
