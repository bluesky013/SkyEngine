//
// Created by blues on 2026/3/10.
//

#pragma once

#include <editor/framework/AssetCreator.h>

namespace sky::editor {

    class ShaderGraphCreator : public AssetCreatorBase {
    public:
        ShaderGraphCreator() = default;
        ~ShaderGraphCreator() override = default;

    private:
        void CreateAsset(const FilePath& path) override;
        std::string GetExtension() const override { return ".shadergraph"; }
    };

} // namespace sky::editor
