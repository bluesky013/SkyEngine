//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/lod/LodMeshPrimitive.h>
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
        explicit MeshRenderer(RenderScene *inScene);
        virtual ~MeshRenderer();

        virtual void Init();

        void SetMeshLodGroup(const RDLodGroupPtr &inGroup);
        void SetDebugFlags(const MeshDebugFlags& flag);

        void UpdateTransform(const Matrix4 &matrix);
    protected:
        void InitUBO();

        void ResetPrimitive();
        void BuildPrimitive(const RDLodGroupPtr &inGroup);
        RenderScene *scene = nullptr;

        RDDynamicUniformBufferPtr ubo;
        MeshDebugFlags debugFlags;

        RDResourceGroupPtr instanceSet;

        RenderLodPrimitive* lodPrimitive = nullptr;
        std::unique_ptr<RenderPrimitive> primitive;
    };

} // namespace sky
