//
// Created by blues on 2023/12/18.
//

#pragma once

#include <render/resource/Buffer.h>
#include <render/resource/Mesh.h>
#include <render/mesh/StandardMeshDefines.h>
#include <render/RenderPrimitive.h>

namespace sky {
    class RenderScene;

    class SkyBoxRenderer {
    public:
        SkyBoxRenderer() = default;
        ~SkyBoxRenderer();

        void SetUp(const RDMaterialInstancePtr &mat);
        void AttachScene(RenderScene *scene);

    private:
        RenderScene *scene;
        RDMaterialInstancePtr material;
        std::unique_ptr<RenderPrimitive> primitive;
    };

} // namespace sky