//
// Aurora light types: the scene light component plus render-facing light
// data. Lives in the aurora/core light submodule (not a separate target);
// SceneTypes.h forwards these so scene consumers keep including it.
//

#pragma once

#include <core/ecs/TypeId.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>

#include <cstdint>

namespace sky::aurora {

    enum class LightType : uint8_t {
        DIRECTIONAL = 0,
        POINT,
        SPOT,
    };

    // Scene light component. World position / direction are derived from the
    // entity's WorldInfo transform (see ExtractLightPosition/Direction); only
    // the intrinsic point/spot parameters live on the component.
    struct Light {
        LightType type      = LightType::DIRECTIONAL;
        Vector3   color     = {1.f, 1.f, 1.f};
        float     intensity = 1.f;

        float range          = 10.f;      // point / spot attenuation radius
        float innerConeAngle = 0.f;       // spot (radians)
        float outerConeAngle = 0.785398f; // spot (radians, ~45 deg)
    };

    // Marks the scene's main directional light. Authored ECS component placed
    // on the entity that carries the primary Light{DIRECTIONAL}; the lighting
    // pipeline reads it (together with Light + WorldInfo) to find the key light.
    struct MainLight {
        bool castShadow = true;
    };

    // GPU-facing packed light record, produced by LocalLightSystem for the
    // caller-owned light buffer. Not an ECS component.
    struct LightRenderData {
        Vector4 position;  // xyz: world position, w: light type (0=directional, 1=point, 2=spot)
        Vector4 color;     // rgb: color, a: intensity
        Vector4 direction; // xyz: world direction, w: unused
        Vector4 params;    // x: range, y: inner cone angle, z:
    };

} // namespace sky::aurora

SKY_TYPE_TAG(sky::aurora::Light, "sky.aurora.Light")
SKY_TYPE_TAG(sky::aurora::MainLight, "sky.aurora.MainLight")
