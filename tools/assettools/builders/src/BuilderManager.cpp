//
// Created by Zach Lee on 2022/8/12.
//

#include <builders/BuilderManager.h>
#include <filesystem>

namespace sky {

    void BuilderManager::RegisterBuilder(BuilderBase* builder)
    {
        builders.emplace_back(builder);
        auto ptr = builders.back().get();

        auto& extensions = ptr->GetExtensions();
        for (auto& ext : extensions) {
            extMap.emplace(ext, builder);
        }
    }

    void BuilderManager::Build(const std::string& projectPath, const std::filesystem::path& path)
    {
        std::filesystem::path filePath(path);
        if (filePath.has_extension()) {
            auto ext = filePath.extension().string();
            auto iter = extMap.find(ext);
            if (iter != extMap.end()) {
                iter->second->Build(projectPath, path);
            }
        }
    }

}