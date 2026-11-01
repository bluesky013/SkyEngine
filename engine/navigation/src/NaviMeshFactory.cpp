//
// Created on 2026/10/30.
//

#include <navigation/NaviMeshFactory.h>

namespace sky::ai {
    bool NaviMeshFactory::Register(Impl *impl, std::shared_ptr<const void> lease)
    {
        if (!impl || factory) {
            delete impl;
            return false;
        }
        codeLease = std::move(lease);
        factory.reset(impl);
        return true;
    }
    void NaviMeshFactory::UnRegister()
    {
        factory.reset();
        codeLease.reset();
    }
    CounterPtr<NaviMesh> NaviMeshFactory::CreateNaviMesh()
    {
        CounterPtr<NaviMesh> result = factory ? factory->CreateNaviMesh() : nullptr;
        if (result)
            result->SetCodeLease(codeLease);
        return result;
    }
    CounterPtr<NaviMeshGenerator> NaviMeshFactory::CreateGenerator()
    {
        CounterPtr<NaviMeshGenerator> result = factory ? factory->CreateGenerator() : nullptr;
        if (result)
            result->SetCodeLease(codeLease);
        return result;
    }
    NaviQueryFilterPtr NaviMeshFactory::CreateQueryFilter()
    {
        NaviQueryFilterPtr result = factory ? factory->CreateQueryFilter() : nullptr;
        if (result)
            result->SetCodeLease(codeLease);
        return result;
    }
} // namespace sky::ai
