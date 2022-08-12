//
// Created by Zach Lee on 2022/8/12.
//

#pragma once

#include <memory>
#include <list>
#include <unordered_map>
#include <builders/BuilderBase.h>

namespace sky {

    class BuilderManager {
    public:
        BuilderManager() = default;
        ~BuilderManager() = default;

        void RegisterBuilder(BuilderBase* builder);

        void Build(const std::string& path);

    private:
        std::list<std::unique_ptr<BuilderBase>> builders;
        std::unordered_map<std::string, BuilderBase*> extMap;
    };

}