//
// Created by blues on 2024/12/7.
//

#pragma once

#include <core/file/FileSystem.h>
#include <core/name/Name.h>
#include <core/environment/Singleton.h>

namespace sky::editor {

    class AssetCreatorBase {
    public:
        AssetCreatorBase() = default;
        virtual ~AssetCreatorBase() = default;

        virtual std::string GetExtension() const = 0;
        virtual void CreateAsset(const FilePath &path) = 0;
    };

    class AssetCreatorManager : public Singleton<AssetCreatorManager> {
    public:
        AssetCreatorManager() = default;
        ~AssetCreatorManager() override = default;

        void RegisterTool(const Name &name, AssetCreatorBase* tool);
        void UnRegisterTool(const Name &name);

        const std::unordered_map<Name, std::unique_ptr<AssetCreatorBase>> &GetTools() const
        {
            return tools;
        }

    private:
        std::unordered_map<Name, std::unique_ptr<AssetCreatorBase>> tools;
    };

} // namespace sky::editor
