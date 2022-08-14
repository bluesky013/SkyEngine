//
// Created by yjrj on 2022/8/8.
//

#include <framework/asset/AssetManager.h>
#include <core/file/FileIO.h>
#include <filesystem>

namespace sky {

    void AssetManager::RegisterAsset(const Uuid& id, const std::string& path)
    {
        pathMap[id] = path;
    }

    void AssetManager::RegisterSearchPath(const std::string& path)
    {
        searchPaths.emplace_back(path);
    }

    std::string AssetManager::GetRealPath(const std::string& relative) const
    {
        std::filesystem::path path(relative);
        if (!std::filesystem::exists(path)) {
            for (auto& sp : searchPaths) {
                std::filesystem::path tmpPath(sp);
                tmpPath.append(path.string());
                if (std::filesystem::exists(tmpPath)) {
                    return tmpPath.string();
                }
            }
        }
        return relative;
    }
}