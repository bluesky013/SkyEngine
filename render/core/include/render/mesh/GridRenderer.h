//
// Created by blues on 2023/12/17.
//


#pragma once

#include <render/resource/Buffer.h>
#include <render/resource/Mesh.h>
#include <render/mesh/StandardMeshDefines.h>

namespace sky {

    class GridRenderer {
    public:
        GridRenderer() = default;
        ~GridRenderer() = default;

        struct Desc {
            uint32_t ext;
        };

        GridRenderer &SetUp(const Desc &desc);
        RDMeshPtr BuildMesh(const RDMaterialInstancePtr &mat);

    private:
        std::vector<RDBufferPtr> vertexBuffers;
        RDBufferPtr indexBuffer;
    };

} // namepace sky