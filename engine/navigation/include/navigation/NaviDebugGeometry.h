//
// Created on 2026/09/21.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Vector4.h>

#include <vector>

namespace sky::ai {

    // Render-agnostic debug geometry: a plain triangle list that any renderer (legacy or aurora)
    // can consume. The navigation core never depends on a renderer.
    struct NaviDebugVertex {
        Vector3 position;
        Vector4 color;
    };

    struct NaviDebugGeometry {
        std::vector<NaviDebugVertex> vertices;
    };

} // namespace sky::ai
