//
// Created by blues on 2025/4/3.
//

#pragma once

#include <string>
#include <shader/ShaderCompilerTypes.h>

namespace sky::sl {

    struct ResourceGroupDecl;

    class ResourceDeclGenerator {
    public:
        ResourceDeclGenerator() = default;
        ~ResourceDeclGenerator() = default;

        std::string Generate(const ResourceGroupDecl &resGroup, ShaderLanguage language);
    };

} // namespace sky::sl