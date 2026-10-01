//
// Aurora scene component types (SoA stored in sparse-set pools).
// Naming avoids the "Component" suffix to stay clear of the framework layer.
//

#pragma once

#include <aurora/light/LightTypes.h>
#include <aurora/resource/Skin.h>
#include <core/ecs/TypeId.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector3.h>
#include <core/shapes/Bounds.h>

namespace sky::aurora {

    // Bounding box + sphere in world space; drives culling and sort depth
    struct Bounds {
        BoundingBoxSphere worldBounds{};
    };

    // world-space placement (plain matrix; TRS composition is the caller's business)
    struct WorldInfo {
        Matrix4 world = Matrix4::Identity();
    };

    struct SkinnedMesh {
        CounterPtr<Skin> skin;
    };

} // namespace sky::aurora

SKY_TYPE_TAG(sky::aurora::Bounds, "sky.aurora.Bounds")
SKY_TYPE_TAG(sky::aurora::WorldInfo, "sky.aurora.WorldInfo")
SKY_TYPE_TAG(sky::aurora::SkinnedMesh, "sky.aurora.SkinnedMesh")
