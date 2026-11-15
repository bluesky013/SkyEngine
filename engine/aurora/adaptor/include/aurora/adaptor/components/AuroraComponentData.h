//
// Created on 2026/11/15.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/util/Uuid.h>

namespace sky::aurora {

    struct StaticMeshComponentData {
        Uuid mesh;
        // Empty uses the mesh material table; an override affects slot zero only.
        Uuid material;
        bool castShadow    = false;
        bool receiveShadow = false;
    };

    enum class SkeletalPoseMode : uint8_t { ReferencePose, SameActorAnimation };

    struct SkeletalMeshComponentData {
        Uuid             mesh;
        Uuid             skin;
        Uuid             skeleton;
        Uuid             material;
        SkeletalPoseMode pose                                                = SkeletalPoseMode::ReferencePose;
        bool             castShadow                                          = true;
        bool             receiveShadow                                       = true;
        bool             operator==(const SkeletalMeshComponentData &) const = default;
    };

    struct LodGroupComponentData {
        Uuid lodGroup;
        bool castShadow    = false;
        bool receiveShadow = false;
    };

    struct CameraComponentData {
        float fov   = 0.785398f; // radians (~45 deg)
        float nearZ = 0.1f;
        float farZ  = 1000.f;
    };

    struct DirectLightData {
        Vector3 color      = {1.f, 1.f, 1.f};
        float   intensity  = 1.f;
        bool    castShadow = true;
    };

    struct PointLightData {
        Vector3 color     = {1.f, 1.f, 1.f};
        float   intensity = 1.f;
        float   range     = 10.f;
    };

    struct SpotLightData {
        Vector3 color          = {1.f, 1.f, 1.f};
        float   intensity      = 1.f;
        float   range          = 10.f;
        float   innerConeAngle = 0.f;
        float   outerConeAngle = 0.785398f; // radians (~45 deg)
    };

    struct EnvironmentData {
        Uuid  diffuseIrradiance, specularEnvironment, brdfIntegration;
        float intensity = 1, exposure = 0;
        bool  enabled = true, hdr = true;
    };

} // namespace sky::aurora
