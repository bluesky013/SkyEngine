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

    struct SphereShape {
        Vector3 pivot = VEC3_ZERO;
        float radius = 1.f;
    };

    struct BoxShape {
        Vector3 pivot = VEC3_ZERO;
        Vector3 halfExt = VEC3_ONE;
    };

    struct CompoundShape {
        std::vector<SphereShape> sphere;
        std::vector<BoxShape> box;
    };

    struct TriangleMeshShape {
        Uuid asset;
    };

    struct MeshPhysicsConfig : public MeshConfigBase {
        std::vector<SphereShape> sphere;
        std::vector<BoxShape> box;
        TriangleMeshShape tris;
    };

    class IShapeImpl {
    public:
        IShapeImpl() = default;
        virtual ~IShapeImpl() = default;

        virtual CounterPtr<TriangleMesh> GetTriangleMesh() const = 0;
    };

} // namespace sky::phy