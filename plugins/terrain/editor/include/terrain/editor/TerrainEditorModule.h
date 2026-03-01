//
// Created by blues on 2024/11/28.
//

#pragma once

#include <terrain/TerrainModule.h>

namespace sky::editor {

    class TerrainEditorModule : public TerrainModule {
    public:
        TerrainEditorModule() = default;
        ~TerrainEditorModule() override = default;

        bool Init(const StartArguments &args) override;
        void Shutdown() override;
    };

} // namespace sky::editor
