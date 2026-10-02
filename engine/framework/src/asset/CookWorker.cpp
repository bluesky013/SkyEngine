//
// Created by blues on 2026/10/2.
//

#include <framework/asset/CookWorker.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetExecutor.h>

namespace sky {

    void CookWorker::CookBatch(const std::vector<Job> &jobs)
    {
        for (const auto &job : jobs) {
            AssetBuilderManager::Get()->BuildRequest(job.uuid, job.target);
        }
        AssetExecutor::Get()->WaitForAll();
    }

    void CookWorker::CookAll()
    {
        // Snapshot source uuids: builders may register dependencies and grow idMap while cooking.
        std::vector<Uuid> uuids;
        for (const auto &[id, info] : AssetDataBase::Get()->GetSources()) {
            uuids.push_back(id);
        }

        for (const auto &id : uuids) {
            AssetDataBase::Get()->BuildAllTargets(id);
        }
        AssetExecutor::Get()->WaitForAll();
    }

} // namespace sky
