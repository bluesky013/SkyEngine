//
// Created by Zach Lee on 2023/2/20.
//

#pragma once
#include <string>
#include <vector>
#include <core/util/Uuid.h>

namespace sky {

    struct BuildProduct {
        std::string productKey;
        Uuid uuid;
    };

    struct BuildRequest {
        std::string fullPath;
        std::string name;
        std::string ext;
        std::string projectDir;
        std::string buildKey;
    };

    struct BuildResult {
        std::vector<BuildProduct> products;
    };

    class AssetBuilder {
    public:
        AssetBuilder() = default;
        virtual ~AssetBuilder() = default;

        virtual void Request(const BuildRequest &build, BuildResult &result) = 0;
    };

}