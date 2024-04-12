//
// Created by bluesky on 2023/4/15.
//

#pragma once

#include <string>

namespace sky {

    class AssetStream {
    public:
        AssetStream(const std::string &p) : path(p) {}
        ~AssetStream() = default;

        std::string ReadString();

    protected:
        std::string path;
    };

}