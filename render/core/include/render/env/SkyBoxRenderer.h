//
// Created by blues on 2023/12/18.
//

#pragma once

#include <render/resource/Buffer.h>
#include <render/resource/Mesh.h>
#include <render/mesh/StandardMeshDefines.h>

namespace sky {

    class SkyBoxRenderer {
    public:
        SkyBoxRenderer() = default;
        ~SkyBoxRenderer() = default;

        void SetUp(const RDTextureCubePtr &cube);

    private:
        std::vector<RDBufferPtr> vertexBuffers;
        RDBufferPtr indexBuffer;
        RDTextureCubePtr cubeMap;
    };

} // namespace sky