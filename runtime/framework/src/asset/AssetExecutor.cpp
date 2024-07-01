//
// Created by blues on 2024/6/29.
//

#include <framework/asset/AssetExecutor.h>

namespace sky {

    void AssetExecutor::WaitForAll()
    {
        executor.wait_for_all();
        std::lock_guard<std::mutex> lock(mutex);
        SKY_ASSERT(savingTasks.empty());
    }

} // namespace sky