//
// Created by Zach Lee on 2022/9/14.
//

#pragma once

#include <builders/BuilderBase.h>

namespace sky {

    class ShaderBuilder : public BuilderBase {
    public:
        ShaderBuilder();
        virtual ~ShaderBuilder() = default;

        virtual const std::vector<std::string>& GetExtensions() const override;

        virtual void Build(const std::string& projectPath, const std::filesystem::path& path) const override;
    };

}