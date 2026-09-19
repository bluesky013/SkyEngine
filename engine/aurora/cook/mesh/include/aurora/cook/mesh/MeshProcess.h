//
// Mesh cook pipeline: shared intermediate representations and the process
// stage base class (mirrors aurora/cook/image/ImageProcess.h).
//

#pragma once

#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/math/Matrix4.h>
#include <core/shapes/AABB.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::aurora::cook {

    // ---- source-space representation (per aiMesh, before interleave) ----

    struct MeshSourcePrimitive {
        std::vector<Vector3> positions;
        std::vector<Vector3> normals;
        std::vector<Vector4> tangents; // w = bitangent sign
        std::vector<Vector2> uvs;      // UV1
        std::vector<Vector4> colors;   // vertex color 0
        std::vector<Vector4> joints;   // global bone indices (as float for simplicity pre-pack)
        std::vector<Vector4> weights;

        std::vector<uint32_t> indices;     // local to this primitive
        uint32_t              materialIndex = 0; // index into CookMeshSource::materialNames
        Matrix4               transform;         // baked node transform
    };

    struct CookMeshSource {
        std::vector<MeshSourcePrimitive> primitives;
        std::vector<std::string>         materialNames; // slot table (uuid wiring lands with material cook)
        SkinAssetData                    skin;          // inverseBindMatrices + boneNames; empty when not skinned

        bool Skinned() const { return !skin.boneNames.empty(); }
        bool Valid() const
        {
            for (const auto &prim : primitives) {
                if (!prim.positions.empty() && !prim.indices.empty()) {
                    return true;
                }
            }
            return false;
        }
    };

    // ---- cooked representation (single interleaved stream) ----

    struct CookedMesh {
        uint32_t vertexStride = 0;
        uint32_t vertexCount  = 0;
        uint32_t indexCount   = 0;
        uint32_t indexType    = 0; // aurora::IndexType

        std::vector<MeshVertexAttributeData> attributes;
        std::vector<uint8_t>                 vertexData;
        std::vector<uint8_t>                 indexData;
        std::vector<MeshSubMeshData>         subMeshes;
        AABB                                 bounds;

        std::vector<MeshletData>       meshlets;
        std::vector<uint32_t>          meshletVertices;
        std::vector<uint8_t>           meshletTriangles;
        std::vector<MeshletBoundsData> meshletBounds;
    };

    class MeshProcess {
    public:
        MeshProcess()          = default;
        virtual ~MeshProcess() = default;

        virtual void DoWork() = 0;
    };

} // namespace sky::aurora::cook
