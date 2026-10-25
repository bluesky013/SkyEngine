//
// Mesh cook pipeline: shared intermediate representations and the process
// stage base class (mirrors aurora/cook/image/ImageProcess.h).
//

#pragma once

#include <aurora/adaptor/assets/MeshAsset.h>
#include <core/math/Matrix4.h>
#include <core/math/Transform.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
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
        std::vector<Vector4> joints;   // ordered Skin slots, packed only after validation
        std::vector<Vector4> weights;

        std::vector<uint32_t> indices;           // local to this primitive
        uint32_t              materialIndex = 0; // index into CookMeshSource::materialNames
        Matrix4               transform;         // baked node transform
    };

    enum class MeshCookError : uint8_t { None, InvalidData, Unsupported };
    struct MeshCookStatus {
        MeshCookError error = MeshCookError::None;
        std::string   message;
        explicit      operator bool() const
        {
            return error == MeshCookError::None;
        }
    };

    struct CookBone {
        std::string name;
        uint16_t    parentIndex = 0xFFFF;
        Transform   refTransform;
    };

    struct CookMeshSource {
        std::vector<MeshSourcePrimitive> primitives;
        std::vector<std::string>         materialNames; // slot table (uuid wiring lands with material cook)
        std::string                      key;
        SkinData                         skin;
        std::vector<CookBone>            bones;

        bool Skinned() const
        {
            return !skin.binds.empty();
        }
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
        SkinInputLayout                      skinInput;
        std::vector<MeshSkinSlotBounds>      slotBounds;
    };

    MeshCookStatus ValidateMeshSource(const CookMeshSource &source);
    MeshCookStatus ValidateCookedMesh(const CookedMesh &mesh);
    MeshCookStatus RefreshMeshBounds(CookedMesh &mesh);

    class MeshProcess {
    public:
        MeshProcess()          = default;
        virtual ~MeshProcess() = default;

        virtual MeshCookStatus DoWork() = 0;
    };

} // namespace sky::aurora::cook
