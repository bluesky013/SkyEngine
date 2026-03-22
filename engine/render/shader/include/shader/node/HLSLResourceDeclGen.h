//
// Created by blues on 2025/4/21.
//

#pragma once

#include <string>

namespace sky::sl {

    struct ResourceGroupDecl;

    class HLSLResourceDeclGenerator {
    public:
        std::string Generate(const ResourceGroupDecl &resGroup);
    };

} // namespace sky::sl
