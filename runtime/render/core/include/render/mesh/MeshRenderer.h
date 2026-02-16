//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/lod/MeshLodGroup.h>
#include <render/mesh/MeshletDebugRender.h>
#include <render/RenderPrimitive.h>
#include <core/math/Matrix4.h>

namespace sky {
    class RenderScene;
    class MeshFeature;

    enum class MeshDebugFlagBit : uint32_t {
        MESHLET = 0x01,
        MESHLET_CONE = 0x02
    };
    using MeshDebugFlags = Flags<MeshDebugFlagBit>;

    class MeshRenderer {
    public:
        MeshRenderer() = default;
        virtual ~MeshRenderer();

        void Tick();
        void AttachScene(RenderScene *scn);
        void SetMesh(const RDMeshPtr &mesh, bool meshShading = false);
        void SetLodGroup(const RDMeshLodGroupPtr &lodGroup);
        void SetDebugFlags(const MeshDebugFlags& flag);

        void UpdateTransform(const Matrix4 &matrix);
        void UpdateLod(const Vector3 &viewPos, float fov, float screenHeight);

        void BuildGeometry();

        void SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh);

        uint32_t GetCurrentLod() const { return currentLod; }
    protected:
        virtual void PrepareUBO();
        virtual RDResourceGroupPtr RequestResourceGroup(MeshFeature *feature);
        virtual void FillVertexFlags(RenderVertexFlags &flags) {}

        void SetupDebugMeshlet();
        void Reset();

        RenderScene *scene = nullptr;

        RDMeshPtr mesh;
        RDMeshLodGroupPtr lodGroup;
        uint32_t currentLod = 0;

        std::vector<std::unique_ptr<RenderMaterialPrimitive>> primitives;
        std::unique_ptr<MeshletDebugRender> meshletDebug;
        std::vector<RDDynamicUniformBufferPtr> meshletInfos;
        RDDynamicUniformBufferPtr ubo;

        bool enableMeshShading = false;
        MeshDebugFlags debugFlags;
    };

} // namespace sky
