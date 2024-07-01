//
// Created by Zach on 2022/8/8.
//

#include <framework/asset/Asset.h>

namespace sky {

    void AssetBase::AddDependencies(const Uuid &id)
    {
        dependencies.emplace_back(id);
    }

    void AssetBase::BlockUntilLoaded() const
    {
        if (asyncTask.second.valid()) {
            asyncTask.second.wait();
        }
    }

} // namespace sky