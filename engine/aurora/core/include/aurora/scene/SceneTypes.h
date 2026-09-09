//
// Aurora scene component types (SoA stored in sparse-set pools).
// Naming avoids the "Component" suffix to stay clear of the framework layer.
//

#pragma once

#include <core/name/Name.h>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>
#include <aurora/rdg/CompiledGraph.h>

namespace sky::aurora {

    // AABB in world space; drives culling and sort depth
    struct Bounds {
        AABB worldBounds{};
    };

    // interim render payload per entity (replaced once the technique design lands)
    struct RenderItem {
        Name     techniqueTag; // matched against SceneRasterQueue::techniqueTag
        DrawItem item;         // pso / batchResourceGroup / vb / ib / offsets / args
    };

    // scene light data (placeholder; lighting pipeline fills in later)
    enum class LightType : uint8_t {
        DIRECTIONAL = 0,
        POINT,
        SPOT,
    };

    struct Light {
        LightType type = LightType::DIRECTIONAL;
        Vector3   color{1.f, 1.f, 1.f};
        Vector3   direction{0.f, -1.f, 0.f};
        float     intensity = 1.f;
    };

    // skinning data (placeholder; skinning pipeline fills in later)
    struct Skin {
        uint32_t jointCount = 0;
    };

} // namespace sky::aurora
