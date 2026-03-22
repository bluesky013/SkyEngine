//
// Created by blues on 2025/4/21.
//

#pragma once

#include <shader/node/ShaderNode.h>
#include <span>
#include <string_view>
#include <vector>

namespace sky::sl {

    struct ConditionalBlock {
        std::string_view              condition;
        std::span<const ResourceDecl> resources;
    };

    struct ResourceGroupDecl {
        std::string_view                     name;
        uint32_t                             setIndex = 0;
        rhi::ShaderStageFlags                defaultVisibility = {};

        std::span<const StructDecl>          localStructDecls;
        std::span<const ResourceDecl>        resources;
        std::span<const ConditionalBlock>    conditionals;
    };

    // Find a struct declaration within a group's local structs
    const StructDecl *FindStructInGroup(std::string_view name, const ResourceGroupDecl &group);

    // Collect all structs referenced by a group's resources (dependency-ordered)
    std::vector<const StructDecl*> CollectReferencedStructs(const ResourceGroupDecl &group);

} // namespace sky::sl
