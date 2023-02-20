//
// Created by Zach Lee on 2023/2/20.
//

#pragma once
#include <string>

namespace sky {

    struct BuildRequest {
        std::string fullPath;
        std::string name;
        std::string ext;
        std::string projectDir;
    };

    class AssetBuilder {
    public:
        AssetBuilder() = default;
        virtual ~AssetBuilder() = default;

        virtual void Request(const BuildRequest &build) = 0;
    };

}