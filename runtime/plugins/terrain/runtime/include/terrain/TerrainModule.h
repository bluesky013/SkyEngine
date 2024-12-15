//
// Created by blues on 2024/11/28.
//

#pragma once

#include <framework/interface/IModule.h>

namespace sky {

    class TerrainModule : public IModule {
    public:
        TerrainModule() = default;
        ~TerrainModule() override = default;

        bool Init(const StartArguments &args) override;

        void Start() override
        {
        }

        void Shutdown() override;
    };

} // namespace sky