//
// Created by blues on 2024/10/11.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/interface/IWorldBuilder.h>
#include <navigation/NaviMesh.h>
#include <navigation/NaviMeshGenerator.h>

namespace sky::ai {

    class NaviMeshFactory : public Singleton<NaviMeshFactory>, public IWorldBuilderGather {
    public:
        NaviMeshFactory();
        ~NaviMeshFactory() override = default;

        CounterPtr<NaviMesh> CreateNaviMesh();
        CounterPtr<NaviMeshGenerator> CreateGenerator();
        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual NaviMesh* CreateNaviMesh() = 0;
            virtual NaviMeshGenerator* CreateGenerator() = 0;
        };
        void Register(Impl* impl);

    private:
        void Gather(std::list<CounterPtr<IWorldBuilder>> &builders) const override;

        std::unique_ptr<Impl> factory;
        EventBinder<IWorldBuilderGather> binder;
    };

} // namespace sky::ai
