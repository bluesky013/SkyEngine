//
// Created by blues on 2023/12/17.
//

#pragma once

#include <cstdint>
#include <core/template/Cast.h>

namespace sky {

    enum class VertexSemantic : uint32_t {
        POSITION,
        UV,
        NORMAL,
        TANGENT,
        COLOR,
        MAX
    };

} // namespace sky