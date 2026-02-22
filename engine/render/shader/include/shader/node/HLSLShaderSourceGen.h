//
// Created by blues on 2025/4/21.
//

#pragma once

#include <shader/node/ShaderGenerator.h>

namespace sky::sl {

    class HLSLShaderGenerator : public ShaderGenerator::Impl {
    public:
        HLSLShaderGenerator() = default;
        ~HLSLShaderGenerator() override = default;

    private:
        std::string Generate(const ResourceGroupDecl& resGroup, uint32_t groupID) override;
    };

} // namespace sky::sl
