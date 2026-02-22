//
// Created by Zach Lee on 2026/1/11.
//

#pragma once

#include <editor/framework/AssetCreator.h>

namespace sky::editor {

    class AnimationGraphCreator : public AssetCreatorBase {
    public:
        AnimationGraphCreator() = default;
        ~AnimationGraphCreator() override = default;

    private:
        void CreateAsset(const FilePath &path) override;
        std::string GetExtension() const override { return ".graph"; }
    };

} // namespace sky::editor
