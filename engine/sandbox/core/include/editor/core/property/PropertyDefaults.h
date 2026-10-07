//
// Created on 2026/10/07.
//

#pragma once

#include <framework/serialization/Any.h>

namespace sky {
    struct TypeInfoRT;
} // namespace sky

namespace sky::editor {

    // A default-constructed instance of a reflected type. This is the baseline the
    // property UI resets against ("reset to default", UE/Godot-style), so a value
    // that differs from the type default keeps its reset affordance across
    // load/save. Returns an empty Any when the type is unknown or has no default
    // constructor.
    Any MakeDefaultValue(const sky::TypeInfoRT *type);

} // namespace sky::editor
