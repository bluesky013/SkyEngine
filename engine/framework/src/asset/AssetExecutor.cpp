//
// Created by blues on 2024/6/29.
//

#include <framework/asset/AssetExecutor.h>

#include <algorithm>
#include <thread>

namespace sky {

    AssetExecutor::AssetExecutor()
        : pool(std::max(1U, std::thread::hardware_concurrency()))
        , cookPool(std::max(1U, std::thread::hardware_concurrency()))
    {
    }

    void AssetExecutor::WaitForAll()
    {
        pool.WaitIdle();
        cookPool.WaitIdle();
        std::lock_guard<std::mutex> lock(mutex);
        SKY_ASSERT(savingTasks.empty());
    }

} // namespace sky
