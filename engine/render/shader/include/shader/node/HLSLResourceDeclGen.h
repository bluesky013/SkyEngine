//
// Created by blues on 2025/4/21.
//

#pragma once

#include <shader/node/ResourceDeclGenerator.h>

namespace sky::sl {

    class HLSLResourceDeclGenerator : public ResourceDeclGenerator::Impl {
    public:
        HLSLResourceDeclGenerator() = default;
        ~HLSLResourceDeclGenerator() override = default;

    private:
        std::string Generate(const ResourceGroupDecl &resGroup) override;
    };

} // namespace sky::sl
