//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/mesh/MeshletDebugRender.h>
#include <render/RenderPrimitive.h>
#include <core/math/Matrix4.h>

namespace sky {
    class RenderScene;
    class MeshFeature;

    enum class MeshDebugFlagBit : uint32_t {
        MESHLET = 0x01,
        MESHLET_CONE = 0x02,
        MESH = 0x04
    };
    using MeshDebugFlags = Flags<MeshDebugFlagBit>;

    class MeshRenderer {
    public:
        MeshRenderer() = default;
        virtual ~MeshRenderer();

        void Tick();
        void AttachScene(RenderScene *scn);
        void SetMesh(const RDMeshPtr &mesh, bool meshShading = false);
        void SetDebugFlags(const MeshDebugFlags& flag);

        void UpdateTransform(const Matrix4 &matrix);
        const Matrix4& GetTransform() const;

        void BuildGeometry();

        void BuildMultipleInstance(uint32_t w, uint32_t h, uint32_t d);

        void SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh);
    protected:
        virtual void PrepareUBO();
        virtual RDResourceGroupPtr RequestResourceGroup(MeshFeature *feature);
        virtual void FillVertexFlags(RenderVertexFlags &flags) {}

        void SetupDebugMeshlet();
        void Reset();

        RenderScene *scene = nullptr;

        RDMeshPtr mesh;
        std::vector<std::unique_ptr<RenderMaterialPrimitive>> primitives;
        std::unique_ptr<MeshletDebugRender> meshletDebug;
        std::vector<RDDynamicUniformBufferPtr> meshletInfos;
        RDDynamicUniformBufferPtr ubo;

        bool enableMeshShading = false;
        RenderGeometryPtr ownGeometry;
        MeshDebugFlags debugFlags;

        RDBufferPtr instanceBuffer;
    };

} // namespace sky
