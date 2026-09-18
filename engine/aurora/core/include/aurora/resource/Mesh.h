//
// Mesh: high-level mesh resource. Wraps a RenderGeometry (vertex streams +
// index + bounds) with sub-meshes (multi-material sections), blend shapes
// (morph targets) and an optional skeleton for skinning. Pure wrapper: the
// geometry owns the GPU buffers, the mesh adds segmentation + animation data.
//

#pragma once

#include <aurora/resource/RenderGeometry.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector3.h>
#include <core/name/Name.h>
#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>

#include <utility>
#include <vector>

namespace sky::aurora {

    // One drawable section of a mesh: a vertex range + index range bound to a
    // material (technique) slot.
    struct SubMesh {
        uint32_t firstVertex   = 0;
        uint32_t vertexCount   = 0;
        uint32_t firstIndex    = 0;
        uint32_t indexCount    = 0;
        uint32_t materialIndex = 0;   // technique/material placeholder index
        AABB     bounds{};
    };

    // Delta-increment morph target: final = base + Σ weightᵢ · deltaᵢ.
    // positionDeltas is required; normal/tangent deltas may be empty (= zero).
    struct BlendShape {
        Name                name;
        std::vector<Vector3> positionDeltas;
        std::vector<Vector3> normalDeltas;
        std::vector<Vector3> tangentDeltas;
    };

    struct Bone {
        Name    name;
        int32_t parent = -1;   // parent bone index (-1 = root)
        Matrix4 inverseBind;   // inverse bind matrix (bind pose)
    };

    class Skeleton : public RefObject {
    public:
        Skeleton() = default;
        ~Skeleton() override = default;

        Skeleton(const Skeleton &) = delete;
        Skeleton &operator=(const Skeleton &) = delete;

        void AddBone(Bone bone)
        {
            bones.push_back(std::move(bone));
        }

        const std::vector<Bone> &GetBones() const { return bones; }

        const Bone *GetBone(uint32_t index) const
        {
            return index < bones.size() ? &bones[index] : nullptr;
        }

        uint32_t GetBoneCount() const { return static_cast<uint32_t>(bones.size()); }

    private:
        std::vector<Bone> bones;
    };

    class Mesh : public RefObject {
    public:
        Mesh() = default;
        explicit Mesh(const Name &inName) : name(inName) {}
        ~Mesh() override = default;

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        void SetGeometry(CounterPtr<RenderGeometry> geo)
        {
            geometry = std::move(geo);
        }

        RenderGeometry *GetGeometry() const { return geometry.Get(); }

        const AABB &GetLocalBounds() const
        {
            static const AABB kEmpty{};
            return geometry != nullptr ? geometry->GetLocalBounds() : kEmpty;
        }

        void AddSubMesh(const SubMesh &sub)
        {
            subMeshes.push_back(sub);
        }

        const std::vector<SubMesh> &GetSubMeshes() const { return subMeshes; }

        void AddBlendShape(BlendShape shape)
        {
            blendShapes.push_back(std::move(shape));
        }

        const std::vector<BlendShape> &GetBlendShapes() const { return blendShapes; }
        uint32_t GetBlendShapeCount() const { return static_cast<uint32_t>(blendShapes.size()); }

        void SetSkeleton(CounterPtr<Skeleton> skel)
        {
            skeleton = std::move(skel);
        }

        CounterPtr<Skeleton> GetSkeleton() const { return skeleton; }
        bool HasSkin() const { return skeleton != nullptr; }

        const Name &GetName() const { return name; }

    private:
        Name                     name;
        CounterPtr<RenderGeometry> geometry;
        std::vector<SubMesh>      subMeshes;
        std::vector<BlendShape>   blendShapes;
        CounterPtr<Skeleton>      skeleton;
    };

} // namespace sky::aurora
