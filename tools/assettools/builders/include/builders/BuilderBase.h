//
// Created by Zach Lee on 2022/8/12.
//

#pragma once

#include <vector>
#include <string>

namespace sky {

    class BuilderBase {
    public:
        BuilderBase() = default;
        virtual ~BuilderBase() = default;

        virtual const std::vector<std::string>& GetExtensions() const = 0;

        virtual void Build(const std::string& projectPath, const std::string& path) const = 0;
    };

}