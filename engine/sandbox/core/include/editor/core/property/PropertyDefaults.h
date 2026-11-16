//
// Created on 2026/10/07.
//

#pragma once

#include <memory>

namespace sky {
    struct TypeInfoRT;
} // namespace sky

namespace sky::editor {

    using DefaultPropertyObject = std::unique_ptr<void, void (*)(void *)>;

    // Own the constructed baseline directly: reflected roots need not be copyable.
    DefaultPropertyObject MakeDefaultObject(const sky::TypeInfoRT *type);

} // namespace sky::editor
