//
// Created by Zach Lee on 2025/10/13.
//

#include <render/resource/StaticMesh.h>

namespace sky {

    void StaticMeshGeometry::Init(uint32_t vertexNum, const Config& config)
    {
        position.reset(new TRawMeshVertexData<Vector3>(vertexNum));
        normal.reset(new TRawMeshVertexData<Vector3>(vertexNum));
        tangent.reset(new TRawMeshVertexData<Vector4>(vertexNum));
        texCoord.reset(new TRawMeshVertexData<Vector2>(vertexNum));

        if (config.HasColor) {
            color .reset(new TRawMeshVertexData<Vector4>(vertexNum));
        }
    }

    void StaticMeshGeometry::Init(uint32_t vertexNum, uint32_t indexNum, rhi::IndexType idxType, const Config& config)
    {
        Init(vertexNum, config);
        OnInit(vertexNum, indexNum, idxType, config);
        indexData = std::make_unique<RawMeshIndexData>(indexNum, idxType);
    }

    void StaticMeshGeometry::SetPosition(uint32_t vertexIndex, const Vector3& val)
    {
        if (position) {
            position->SetVertexData(vertexIndex, val, 0);
        }
    }

    void StaticMeshGeometry::SetTangent(uint32_t vertexIndex, const Vector3& nVal, const Vector4& tVal)
    {
        if (normal) {
            normal->SetVertexData(vertexIndex, nVal, 0);
        }

        if (tangent) {
            tangent->SetVertexData(vertexIndex, tVal, 0);
        }
    }

    void StaticMeshGeometry::SetUv0(uint32_t vertexIndex, const Vector2& uv0)
    {
        if (texCoord) {
            texCoord->SetVertexData(vertexIndex, uv0, 0);
        }
    }

    void StaticMeshGeometry::SetIndex(uint32_t idx, uint32_t val)
    {
        if (indexData) {
            indexData->SetIndex(idx, val);
        }
    }

    void StaticMeshGeometry::AddSubMesh(const MeshSubSection& section)
    {
        sections.emplace_back(section);
    }

} // namespace sky