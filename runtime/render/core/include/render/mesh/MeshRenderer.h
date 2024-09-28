//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/RenderPrimitive.h>
#include <core/math/Matrix4.h>

namespace sky {
    class RenderScene;
    class MeshFeature;

    class MeshRenderer {
    public:

        MeshRenderer() = default;
        virtual ~MeshRenderer();

        void Tick();
        void AttachScene(RenderScene *scn);
        void SetMesh(const RDMeshPtr &mesh);
        void UpdateTransform(const Matrix4 &matrix);

        void SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh);
    protected:
        virtual void PrepareUBO();
        virtual RDResourceGroupPtr RequestResourceGroup(MeshFeature *feature);

        RenderScene *scene = nullptr;

        RDMeshPtr mesh;
        std::vector<std::unique_ptr<RenderPrimitive>> primitives;
        RDDynamicUniformBufferPtr ubo;
    };

} // namespace sky
