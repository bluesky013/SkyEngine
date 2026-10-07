//
// Created on 2026/10/07.
//

#pragma once

#include <core/name/Name.h>
#include <framework/serialization/Any.h>

#include <vector>

namespace sky {

    // One entry in a world's subsystem set: a registry name, an optional reflected
    // config, and whether it is enabled. Order is preserved (subsystems may depend
    // on earlier ones).
    struct WorldSubSystemDesc {
        Name name;
        Any  config;
        bool enabled = true;
    };

    // Declarative description of a world's subsystems.
    struct WorldDesc {
        std::vector<WorldSubSystemDesc> subSystems;
    };

} // namespace sky
