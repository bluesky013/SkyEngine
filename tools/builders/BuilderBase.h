//
// Created by Zach Lee on 2022/3/10.
//


#pragma once
#include <string>

namespace sky {

    struct BuildRequest {
        std::string srcFolder;
        std::string srcFile;
        std::string dstFolder;
        std::string dstFile;
    };

    class BuilderBase {
    public:
        BuilderBase() = default;
        virtual ~BuilderBase() = default;

        virtual bool Build(const BuildRequest&) = 0;
    };

}