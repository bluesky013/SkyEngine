//
// Created by Zach Lee on 2023/2/20.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

namespace sky::builder {

    class PrefabBuilder : public AssetBuilder {
    public:
        PrefabBuilder() = default;
        ~PrefabBuilder() = default;

        void Request(const BuildRequest &build, BuildResult &result) override {}
    };

} // namespace sky::builder