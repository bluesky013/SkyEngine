//
// Created by Zach Lee on 2023/2/20.
//

#pragma once
#include <string>
#include <vector>
#include <core/util/Uuid.h>

namespace sky {

    struct BuildRequest {
        std::string fullPath;
        std::string name;
        std::string ext;
        std::string projectDir;
        std::vector<Uuid> products;
    };

    class AssetBuilder {
    public:
        AssetBuilder() = default;
        virtual ~AssetBuilder() = default;

        virtual void Request(BuildRequest &build) = 0;
    };

}