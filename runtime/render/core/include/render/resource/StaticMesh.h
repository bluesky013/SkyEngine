//
// Created by Zach Lee on 2025/10/12.
//

#pragma once

#include <render/RenderResource.h>
#include <render/resource/Mesh.h>

namespace sky {

    enum class VertexPrecision : uint8_t {
        FLOAT = 0,
        HALF
    };

    template <uint32_t N>
    struct VF_TB_UVN {
        Vector3 normal;
        Vector4 tangent;
        Vector2 texCoord[N];
    };

    class StaticMeshGeometry : public RefObject {
    public:
        struct Config {
            Config()
            {
                posPrecision = VertexPrecision::FLOAT;
                tanPrecision = VertexPrecision::FLOAT;
                UvPrecision = VertexPrecision::FLOAT;
                UVNum = 1;
                HasColor = 0;
            }

            VertexPrecision posPrecision;
            VertexPrecision tanPrecision;
            VertexPrecision UvPrecision;
            uint8_t         UVNum      : 7;
            uint8_t         HasColor   : 1;
        };

        StaticMeshGeometry() = default;
        ~StaticMeshGeometry() override = default;

        void Init(uint32_t vertexNum, const Config& config = {});
        void Init(uint32_t vertexNum, uint32_t indexNum, rhi::IndexType idxType, const Config& config = {});

        void SetPosition(uint32_t vertexIndex, const Vector3& position);
        void SetTangent(uint32_t vertexIndex, const Vector3& normal, const Vector4& tangent);
        void SetUv0(uint32_t vertexIndex, const Vector2& uv0);

        void SetIndex(uint32_t idx, uint32_t val);

        void AddSubMesh(const MeshSubSection& section);

        MeshVertexDataInterface* GetPositionBuffer() const { return position.get(); }
        MeshVertexDataInterface* GetNormalBuffer() const { return normal.get(); }
        MeshVertexDataInterface* GetTangentBuffer() const { return tangent.get(); }
        MeshVertexDataInterface* GetTexCoordBuffer(uint32_t index = 0) const { return texCoord.get(); }
        MeshVertexDataInterface* GetColorBuffer() const { return color.get(); }
        RawMeshIndexData* GetIndexBuffer() const { return indexData.get(); }

        const std::vector<MeshSubSection> &GetSubMeshes() const { return sections; }
    protected:
        virtual void OnInit(uint32_t vertexNum, uint32_t indexNum, rhi::IndexType idxType, const Config& config) {}

        std::unique_ptr<MeshVertexDataInterface> position;
        std::unique_ptr<MeshVertexDataInterface> normal;
        std::unique_ptr<MeshVertexDataInterface> tangent;
        std::unique_ptr<MeshVertexDataInterface> texCoord;
        std::unique_ptr<MeshVertexDataInterface> color;

        std::unique_ptr<RawMeshIndexData> indexData;

        std::vector<MeshSubSection> sections;
    };

} // namespace sky