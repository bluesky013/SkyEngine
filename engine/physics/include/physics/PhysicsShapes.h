//
// Created on 2026/09/23.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/shapes/TriangleMesh.h>
#include <core/util/Uuid.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::phy {

    enum class ShapeType : uint8_t {
        Box = 0,
        Sphere,
        Capsule,
        HeightField,
        TriangleMesh,
        ConvexHull,
        Compound
    };

    // Backend-neutral shape description. A single tagged struct keeps serialization and editing simple
    // and avoids exposing any backend shape type; unused members are ignored per `type`.
    struct ShapeDesc {
        ShapeType type  = ShapeType::Box;
        Vector3   pivot = VEC3_ZERO;

        // Box
        Vector3 halfExt = VEC3_ONE;

        // Sphere / Capsule / cylinder-based shapes
        float radius = 0.5f;
        float height = 1.f; // capsule cylindrical section, excludes the two caps

        // HeightField: cols x rows grid of world-space heights (row-major, z * cols + x)
        uint32_t           cols = 0;
        uint32_t           rows = 0;
        std::vector<float> samples;
        float              scaleX       = 1.f;
        float              scaleZ       = 1.f;
        float              heightScale  = 1.f;
        float              heightOffset = 0.f;
        float              minHeight    = 0.f;
        float              maxHeight    = 0.f;
        uint8_t            upAxis       = 1; // 0 = X, 1 = Y, 2 = Z

        // TriangleMesh: asset reference plus optional runtime-cooked data (render-agnostic)
        Uuid                     mesh;
        CounterPtr<TriangleMesh> meshData;

        // ConvexHull
        std::vector<Vector3> points;

        // Compound
        std::vector<ShapeDesc> children;
    };

    // Pure validation used by editors and by backends before cooking. Returns false and fills `why`
    // (when provided) for malformed descriptors.
    bool ValidateShape(const ShapeDesc &desc, std::string *why = nullptr);

} // namespace sky::phy
