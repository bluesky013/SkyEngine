// Mesh owns geometry and segmentation. Instances select independent Skin bindings.

#pragma once

#include <aurora/resource/RenderGeometry.h>
#include <core/math/Vector3.h>
#include <core/name/Name.h>
#include <core/shapes/Bounds.h>
#include <core/template/ReferenceObject.h>

#include <utility>
#include <vector>

namespace sky::aurora {

    // One drawable section of a mesh: a vertex range + index range bound to a
    // material (technique) slot.
    struct SubMesh {
        uint32_t          firstVertex   = 0;
        uint32_t          vertexCount   = 0;
        uint32_t          firstIndex    = 0;
        uint32_t          indexCount    = 0;
        uint32_t          materialIndex = 0; // technique/material placeholder index
        BoundingBoxSphere bounds{};
    };

    // Delta-increment morph target: final = base + sum(weight[i] * delta[i]).
    // positionDeltas is required; normal/tangent deltas may be empty (= zero).
    struct BlendShape {
        Name                 name;
        std::vector<Vector3> positionDeltas;
        std::vector<Vector3> normalDeltas;
        std::vector<Vector3> tangentDeltas;
    };

    class Mesh : public RefObject {
    public:
        Mesh() = default;
        explicit Mesh(const Name &inName) : name(inName)
        {
        }
        ~Mesh() override = default;

        Mesh(const Mesh &)            = delete;
        Mesh &operator=(const Mesh &) = delete;

        void SetGeometry(CounterPtr<RenderGeometry> geo)
        {
            geometry = std::move(geo);
        }

        RenderGeometry *GetGeometry() const
        {
            return geometry.Get();
        }

        const BoundingBoxSphere &GetLocalBounds() const
        {
            static const BoundingBoxSphere kEmpty{};
            return geometry != nullptr ? geometry->GetLocalBounds() : kEmpty;
        }

        void AddSubMesh(const SubMesh &sub)
        {
            subMeshes.push_back(sub);
        }

        const std::vector<SubMesh> &GetSubMeshes() const
        {
            return subMeshes;
        }

        void AddBlendShape(BlendShape shape)
        {
            blendShapes.push_back(std::move(shape));
        }

        const std::vector<BlendShape> &GetBlendShapes() const
        {
            return blendShapes;
        }
        uint32_t GetBlendShapeCount() const
        {
            return static_cast<uint32_t>(blendShapes.size());
        }

        const Name &GetName() const
        {
            return name;
        }

    private:
        Name                       name;
        CounterPtr<RenderGeometry> geometry;
        std::vector<SubMesh>       subMeshes;
        std::vector<BlendShape>    blendShapes;
    };

} // namespace sky::aurora
