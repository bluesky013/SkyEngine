//
// MeshAssembler (see MeshAssembler.h).
//

#include <aurora/cook/mesh/MeshAssembler.h>

#include <aurora/rhi/Core.h>
#include <aurora/rhi/VertexSemantic.h>
#include <core/logger/Logger.h>

#include <cstring>
#include <limits>

static const char *TAG = "AuroraMeshCook";

namespace sky::aurora::cook {

    namespace {

        struct AttributePlan {
            VertexSemantic semantic;
            uint32_t       format; // Format
            uint32_t       size;   // bytes
        };

        uint32_t AttributeSize(VertexSemantic semantic)
        {
            switch (semantic) {
            case VertexSemantic::POSITION:
            case VertexSemantic::NORMAL: return 12;
            case VertexSemantic::TANGENT:
            case VertexSemantic::COLOR:
            case VertexSemantic::WEIGHTS: return 16;
            case VertexSemantic::UV1: return 8;
            case VertexSemantic::JOINTS: return 4;
            default: return 0;
            }
        }

        uint32_t AttributeFormat(VertexSemantic semantic)
        {
            switch (semantic) {
            case VertexSemantic::POSITION:
            case VertexSemantic::NORMAL: return static_cast<uint32_t>(Format::F_RGB32);
            case VertexSemantic::TANGENT:
            case VertexSemantic::COLOR:
            case VertexSemantic::WEIGHTS: return static_cast<uint32_t>(Format::F_RGBA32);
            case VertexSemantic::UV1: return static_cast<uint32_t>(Format::F_RG32);
            case VertexSemantic::JOINTS: return static_cast<uint32_t>(Format::U_RGBA8);
            default: return static_cast<uint32_t>(Format::UNDEFINED);
            }
        }

        void WriteAttribute(uint8_t                   *dst,
                            VertexSemantic             semantic,
                            const MeshSourcePrimitive &prim,
                            uint32_t                   vertex,
                            const Matrix4             &transform,
                            const Matrix4             &normalTransform)
        {
            switch (semantic) {
            case VertexSemantic::POSITION: {
                const Vector4 p = transform * Vector4{prim.positions[vertex].x, prim.positions[vertex].y, prim.positions[vertex].z, 1.f};
                std::memcpy(dst, &p.x, 12);
                break;
            }
            case VertexSemantic::NORMAL: {
                Vector3 n = prim.normals.empty() ? Vector3{0.f, 1.f, 0.f} : prim.normals[vertex];
                if (!prim.normals.empty()) {
                    const Vector4 t = normalTransform * Vector4{n.x, n.y, n.z, 0.f};
                    n               = Vector3{t.x, t.y, t.z};
                    n.Normalize();
                }
                std::memcpy(dst, &n.x, 12);
                break;
            }
            case VertexSemantic::TANGENT: {
                Vector4 t = prim.tangents.empty() ? Vector4{1.f, 0.f, 0.f, 1.f} : prim.tangents[vertex];
                if (!prim.tangents.empty()) {
                    const Vector4 r = transform * Vector4{t.x, t.y, t.z, 0.f};
                    Vector3       tn{r.x, r.y, r.z};
                    const auto    sourceNormal      = prim.normals.empty() ? Vector3{0, 1, 0} : prim.normals[vertex];
                    const Vector4 transformedNormal = normalTransform * Vector4{sourceNormal.x, sourceNormal.y, sourceNormal.z, 0};
                    Vector3       normal{transformedNormal.x, transformedNormal.y, transformedNormal.z};
                    normal.Normalize();
                    tn = tn - normal * (normal.x * tn.x + normal.y * tn.y + normal.z * tn.z);
                    tn.Normalize();
                    t = Vector4{tn.x, tn.y, tn.z, t.w * (transform.Determinant() < 0 ? -1.f : 1.f)};
                }
                std::memcpy(dst, &t.x, 16);
                break;
            }
            case VertexSemantic::UV1: {
                const Vector2 uv = prim.uvs.empty() ? Vector2{} : prim.uvs[vertex];
                std::memcpy(dst, &uv.x, 8);
                break;
            }
            case VertexSemantic::COLOR: {
                const Vector4 c = prim.colors.empty() ? Vector4{1.f, 1.f, 1.f, 1.f} : prim.colors[vertex];
                std::memcpy(dst, &c.x, 16);
                break;
            }
            case VertexSemantic::JOINTS: {
                uint8_t packed[4] = {};
                if (!prim.joints.empty()) {
                    for (uint32_t i = 0; i < 4; ++i) {
                        packed[i] = static_cast<uint8_t>(prim.joints[vertex][i]);
                    }
                }
                std::memcpy(dst, packed, 4);
                break;
            }
            case VertexSemantic::WEIGHTS: {
                const Vector4 w = prim.weights.empty() ? Vector4{1.f, 0.f, 0.f, 0.f} : prim.weights[vertex];
                std::memcpy(dst, &w.x, 16);
                break;
            }
            default: break;
            }
        }

    } // namespace

    MeshCookStatus MeshAssembler::DoWork()
    {
        if (!payload.source || !payload.config || !payload.out) {
            return {MeshCookError::InvalidData, "missing mesh assembly input"};
        }
        const auto &source = *payload.source;
        if (const auto status = ValidateMeshSource(source); !status) {
            return status;
        }
        const auto &config = *payload.config;
        auto       &out    = *payload.out;

        out.skinInput = source.skin.input;
        // attribute plan: fixed order, gated by config + source availability
        bool anyNormals  = false;
        bool anyTangents = false;
        bool anyUvs      = false;
        bool anyColors   = false;
        for (const auto &prim : source.primitives) {
            anyNormals  = anyNormals || !prim.normals.empty();
            anyTangents = anyTangents || !prim.tangents.empty();
            anyUvs      = anyUvs || !prim.uvs.empty();
            anyColors   = anyColors || !prim.colors.empty();
        }

        std::vector<VertexSemantic> semantics = {VertexSemantic::POSITION};
        if (anyNormals) {
            semantics.emplace_back(VertexSemantic::NORMAL);
        }
        if (anyTangents && config.tangents) {
            semantics.emplace_back(VertexSemantic::TANGENT);
        }
        if (anyUvs) {
            semantics.emplace_back(VertexSemantic::UV1);
        }
        if (anyColors) {
            semantics.emplace_back(VertexSemantic::COLOR);
        }
        if (source.Skinned()) {
            semantics.emplace_back(VertexSemantic::JOINTS);
            semantics.emplace_back(VertexSemantic::WEIGHTS);
        }

        uint32_t stride = 0;
        out.attributes.clear();
        for (const auto semantic : semantics) {
            MeshVertexAttributeData attr;
            attr.semantic = static_cast<uint8_t>(semantic);
            attr.format   = AttributeFormat(semantic);
            attr.offset   = stride;
            out.attributes.emplace_back(attr);
            stride += AttributeSize(semantic);
        }
        out.vertexStride = stride;

        // counts
        uint32_t totalVertices = 0;
        uint32_t totalIndices  = 0;
        for (const auto &prim : source.primitives) {
            totalVertices += static_cast<uint32_t>(prim.positions.size());
            totalIndices += static_cast<uint32_t>(prim.indices.size());
        }
        out.vertexCount = totalVertices;
        out.indexCount  = totalIndices;
        out.indexType   = totalVertices < 65536 ? static_cast<uint32_t>(IndexType::U16) : static_cast<uint32_t>(IndexType::U32);

        out.vertexData.resize(static_cast<size_t>(totalVertices) * stride);
        out.indexData.resize(static_cast<size_t>(totalIndices) * (out.indexType == static_cast<uint32_t>(IndexType::U16) ? 2 : 4));
        out.subMeshes.clear();
        out.subMeshes.reserve(source.primitives.size());
        out.bounds = AABB{};

        uint32_t vertexBase   = 0;
        uint32_t indexOffset  = 0; // in indices
        uint8_t *vertexCursor = out.vertexData.data();
        auto    *indexCursor  = out.indexData.data();

        for (const auto &prim : source.primitives) {
            const Matrix4 normalTransform = prim.transform.InverseTranspose();
            const auto    vertexCount     = static_cast<uint32_t>(prim.positions.size());

            for (uint32_t v = 0; v < vertexCount; ++v) {
                uint8_t *dst = vertexCursor + static_cast<size_t>(v) * stride;
                for (const auto &attr : out.attributes) {
                    WriteAttribute(dst + attr.offset, static_cast<VertexSemantic>(attr.semantic), prim, v, prim.transform, normalTransform);
                }
            }

            if (out.indexType == static_cast<uint32_t>(IndexType::U16)) {
                auto *dst = reinterpret_cast<uint16_t *>(indexCursor);
                for (uint32_t i = 0; i < prim.indices.size(); ++i) {
                    dst[i] = static_cast<uint16_t>(prim.indices[(prim.transform.Determinant() < 0 && i % 3 != 0) ? i + (i % 3 == 1 ? 1 : -1) : i] +
                                                   vertexBase);
                }
                indexCursor += prim.indices.size() * 2;
            } else {
                auto *dst = reinterpret_cast<uint32_t *>(indexCursor);
                for (uint32_t i = 0; i < prim.indices.size(); ++i) {
                    dst[i] = prim.indices[(prim.transform.Determinant() < 0 && i % 3 != 0) ? i + (i % 3 == 1 ? 1 : -1) : i] + vertexBase;
                }
                indexCursor += prim.indices.size() * 4;
            }

            MeshSubMeshData sub;
            sub.indexOffset   = indexOffset;
            sub.indexCount    = static_cast<uint32_t>(prim.indices.size());
            sub.materialIndex = prim.materialIndex;
            out.subMeshes.emplace_back(sub);

            indexOffset += static_cast<uint32_t>(prim.indices.size());
            vertexBase += vertexCount;
            vertexCursor += static_cast<size_t>(vertexCount) * stride;
        }

        if (const auto status = RefreshMeshBounds(out); !status) {
            return status;
        }
        return ValidateCookedMesh(out);
    }

} // namespace sky::aurora::cook
