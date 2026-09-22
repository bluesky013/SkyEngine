//
// Created by blues on 2024/10/3.
//

#pragma once

#include <framework/interface/IMeshConfigNotify.h>
#include <core/shapes/TriangleMesh.h>
#include <core/math/Vector3.h>
#include <core/template/Flags.h>

namespace sky::phy {

    enum class CollisionFilterBit : uint32_t  {
        DEFAULT       = 0x01,
        DFT_STATIC    = 0x02,
        DFT_DYNAMIC   = 0x04,
        DFT_CHARACTER = 0x08,

        ALL = 0xFFFFFFFF
    };
    using CollisionFilters = Flags<CollisionFilterBit>;
    ENABLE_FLAG_BIT_OPERATOR(CollisionFilterBit)

    // Engine-level filter contract: two objects interact when each one's group is in the other's mask.
    inline bool CollisionFilterAccepts(const CollisionFilters &groupA, const CollisionFilters &maskA,
                                       const CollisionFilters &groupB, const CollisionFilters &maskB)
    {
        return (groupA & maskB).value != 0 && (groupB & maskA).value != 0;
    }

    struct SphereShape {
        Vector3 pivot = VEC3_ZERO;
        float radius = 1.f;
    };

    struct BoxShape {
        Vector3 pivot = VEC3_ZERO;
        Vector3 halfExt = VEC3_ONE;
    };

    struct CapsuleShape {
        Vector3 pivot = VEC3_ZERO;
        float radius = 0.5f;
        float height = 1.f;   // cylindrical section height (excludes the two caps)
    };

    struct CompoundShape {
        std::vector<SphereShape> sphere;
        std::vector<BoxShape> box;
    };

    // Runtime triangle-mesh collision data. Render-agnostic: produced offline (cook) or by the asset
    // pipeline, never loaded from a render mesh at runtime.
    struct TriangleMeshShape {
        CounterPtr<TriangleMesh> mesh;
    };

    // Backend-neutral heightfield: a width x height grid of world-height samples (row-major z * width + x).
    struct HeightFieldShape {
        uint32_t           width  = 0;   // vertices along the first grid axis
        uint32_t           height = 0;   // vertices along the second grid axis
        std::vector<float> samples;      // width * height heights
        float              scaleX = 1.f; // world size per column
        float              scaleZ = 1.f; // world size per row
        float              heightScale  = 1.f;
        float              heightOffset = 0.f;
        float              minHeight = 0.f;
        float              maxHeight = 0.f;
        uint8_t            upAxis = 1;   // 0 = X, 1 = Y, 2 = Z
    };

    struct MeshPhysicsConfig : public MeshConfigBase {
        std::vector<SphereShape> sphere;
        std::vector<BoxShape> box;
        Uuid              mesh;   // serialized reference to a collision mesh asset (render-agnostic)
        TriangleMeshShape tris;   // runtime shape data (resolved from `mesh` by the asset pipeline)
    };

    class IShapeImpl {
    public:
        IShapeImpl() = default;
        virtual ~IShapeImpl() = default;

        virtual CounterPtr<TriangleMesh> GetTriangleMesh() const = 0;
    };

} // namespace sky::phy