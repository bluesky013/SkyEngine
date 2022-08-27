//
// Created by Zach Lee on 2022/8/12.
//

#pragma once

#include <builders/BuilderBase.h>

namespace sky {

    class ModelBuilder : public BuilderBase {
    public:
        ModelBuilder();
        virtual ~ModelBuilder() = default;

        virtual const std::vector<std::string>& GetExtensions() const override;

        virtual void Build(const std::string& projectPath, const std::string& path) const override;
    };

}