//
// Created by Zach Lee on 2022/8/25.
//

#include <render/resources/Prefab.h>
#include <framework/asset/AssetManager.h>
#include <cereal/archives/json.hpp>

namespace sky {


    namespace impl {
        void LoadFromPath(const std::string& path, PrefabData& data)
        {
            auto realPath = AssetManager::Get()->GetRealPath(path);
            std::ifstream file(realPath,  std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            cereal::JSONInputArchive archive(file);
            archive >> data;
        }

        void SaveToPath(const std::string& path, const PrefabData& data)
        {
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            cereal::JSONOutputArchive binOutput(file);
            binOutput << data;
        }

        PrefabPtr CreateFromData(const PrefabData& data)
        {
            return {};
        }
    }
}