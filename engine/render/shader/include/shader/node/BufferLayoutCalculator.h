//
// Created by blues on 2025/4/21.
//

#pragma once

#include <shader/node/ShaderLayoutCalc.h>
#include <span>
#include <string>
#include <vector>

namespace sky::sl {

    struct ResourceGroupDecl;
    struct ResourceDecl;

    class BufferLayoutCalculator {
    public:
        static uint32_t CalculateMembers(
            std::span<const MemberDecl> members,
            const ResourceGroupDecl *group,
            LayoutStandard std,
            std::vector<LayoutInfo> &outLayouts);

        static uint32_t CalculateStructSize(
            const StructDecl &decl,
            const ResourceGroupDecl *group = nullptr);

        struct ValidationMessage {
            enum class Level { WARNING, ERROR };
            Level       level;
            std::string message;
        };

        static std::vector<ValidationMessage> Validate(
            const ResourceDecl &cbuffer,
            const ResourceGroupDecl *group = nullptr);
    };

} // namespace sky::sl
