//
// Created by blues on 2025/2/9.
//

#pragma once

#include <render/adaptor/assets/MeshAsset.h>
#include <builder/render/mesh/ClusterBase.h>

namespace sky::builder {

    class ClusterBuilder {
    public:
        ClusterBuilder() = default;
        ~ClusterBuilder() = default;
        
        void BuildFromMeshData(const TBufferViewAccessor<Vector3>& position, const TBufferViewAccessor<uint32_t>& indices);
    };

} // namespace sky::builder
