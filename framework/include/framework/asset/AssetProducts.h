//
// Created by blues on 2023/12/31.
//

#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>

#include <core/platform/Platform.h>
#include <core/util/Uuid.h>

namespace sky {

    struct ProductAssetInfo {
    };

    using ProductAssetIDMap = std::unordered_map<Uuid, ProductAssetInfo>;

    class AssetProducts {
    public:
        AssetProducts() = default;
        ~AssetProducts() = default;

        void LoadFromFile(const std::string &path);
        void SaveToFile(const std::string &path);

        void AddAsset(const Uuid &uuid, const ProductAssetInfo &info);
        void RemoveAsset(const Uuid &uuid);
        void RegisterDependencies(const Uuid &src, const std::vector<Uuid> &dst);
        bool HasAsset(const Uuid &uuid) const { return assetLists.count(uuid) != 0u; }
        const ProductAssetInfo *GetProduct(const Uuid &uuid) const;
        const ProductAssetIDMap &GetProducts() const { return assetLists; }

    private:
        mutable std::mutex mutex;
        ProductAssetIDMap assetLists;
        std::unordered_map<Uuid, std::vector<Uuid>> dependencies;
    };

} // namespace sky