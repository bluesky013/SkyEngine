//
// Created by blues on 2026/10/3.
//

#include <framework/asset/InProcessCookRunner.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetExecutor.h>
#include <utility>

namespace sky {

    void InProcessCookRunner::SetCompletion(CookCompletion handler)
    {
        completion = std::move(handler);
    }

    bool InProcessCookRunner::Request(const CookJob &job)
    {
        auto *manager = AssetBuilderManager::Get();
        if (manager == nullptr) {
            return false;
        }
        // BuildRequest schedules on the asset/cook pool, broadcasts
        // IAssetEvent::OnAssetBuildFinished, and invokes our completion handler.
        manager->BuildRequest(job.uuid, job.target, completion);
        return true;
    }

    void InProcessCookRunner::Drain()
    {
        AssetExecutor::Get()->WaitForAll();
    }

} // namespace sky
