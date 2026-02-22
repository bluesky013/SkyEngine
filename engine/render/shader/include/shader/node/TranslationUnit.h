//
// Created by blues on 2025/4/3.
//

#pragma once

#include <shader/node/ShaderNode.h>

namespace sky::sl {

    class TranslationUnit {
    public:
        TranslationUnit() = default;
        ~TranslationUnit() = default;

    private:
        NodeBase* rootNode = nullptr;
    };


} // namespace sky::sl