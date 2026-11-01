//
// Created on 2026/10/30.
//

#include <navigation/NaviMeshFactory.h>

namespace sky::ai {
    bool NaviMeshFactory::Register(Impl *impl)
    {
        if (!impl || factory) {
            delete impl;
            return false;
        }
        factory.reset(impl);
        return true;
    }
    void NaviMeshFactory::UnRegister()
    {
        factory.reset();
    }
    CounterPtr<NaviMesh> NaviMeshFactory::CreateNaviMesh()
    {
        return factory ? factory->CreateNaviMesh() : nullptr;
    }
    CounterPtr<NaviMeshGenerator> NaviMeshFactory::CreateGenerator()
    {
        return factory ? factory->CreateGenerator() : nullptr;
    }
    NaviQueryFilterPtr NaviMeshFactory::CreateQueryFilter()
    {
        return factory ? factory->CreateQueryFilter() : nullptr;
    }
} // namespace sky::ai
