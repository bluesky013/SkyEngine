//
// Created on 2026/10/30.
//

#pragma once

#include <core/environment/Singleton.h>
#include <memory>
#include <navigation/NaviMesh.h>
#include <navigation/NaviMeshGenerator.h>

namespace sky::ai {
    class NaviMeshFactory : public Singleton<NaviMeshFactory> {
    public:
        class Impl {
        public:
            virtual ~Impl()                                = default;
            virtual NaviMesh          *CreateNaviMesh()    = 0;
            virtual NaviMeshGenerator *CreateGenerator()   = 0;
            virtual NaviQueryFilter   *CreateQueryFilter() = 0;
        };
        CounterPtr<NaviMesh>          CreateNaviMesh();
        CounterPtr<NaviMeshGenerator> CreateGenerator();
        NaviQueryFilterPtr            CreateQueryFilter();
        bool                          Register(Impl *impl, std::shared_ptr<const void> codeLease = {});
        void                          UnRegister();

    private:
        friend class Singleton<NaviMeshFactory>;
        NaviMeshFactory() = default;
        std::shared_ptr<const void> codeLease;
        std::unique_ptr<Impl>       factory;
    };
} // namespace sky::ai
