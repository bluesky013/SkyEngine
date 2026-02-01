//
// Created by blues on 2024/9/29.
//

#include <render/resource/SkeletonMesh.h>

namespace sky {

    void SkeletalMeshGeometry::SetBoneIndexAndWeight(uint32_t vertexIndex, const VertexBoneData& data)
    {
        if (boneWeightBuffer) {
            boneWeightBuffer->SetVertexData(vertexIndex, data, 0);
        }
    }

    void SkeletalMeshGeometry::OnInit(uint32_t vertexNum, uint32_t indexNum, rhi::IndexType idxType, const Config& config)
    {
        boneWeightBuffer.reset(new TRawMeshVertexData<VertexBoneData>(vertexNum));
    }

} // namespace sky