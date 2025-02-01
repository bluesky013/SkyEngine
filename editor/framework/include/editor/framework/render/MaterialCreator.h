//
// Created by blues on 2024/12/7.
//

#pragma once

#include <editor/framework/AssetCreator.h>

namespace sky::editor {

    class MaterialInstanceCreator : public AssetCreatorBase {
    public:
        MaterialInstanceCreator() = default;
        ~MaterialInstanceCreator() override = default;

    private:
        void CreateAsset(const FilePath &path) override;
        std::string GetExtension() const override { return ".mati"; }
    };

} // namespace sky::editor
