//
// Created by Zach Lee on 2022/7/19.
//

#pragma once
#include <core/logger/Logger.h>
#include <core/math/Matrix.h>
#include <core/util/Macros.h>
#include <render/RenderBufferPool.h>
#include <render/RenderMeshPrimtive.h>
#include <render/resources/DescirptorGroup.h>
#include <render/resources/Material.h>
#include <render/resources/Mesh.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/DrawItem.h>
#include <vulkan/VertexAssembly.h>

namespace sky {
    class RenderScene;
    class RenderView;

    struct ObjectInfo {
        Matrix4 worldMatrix            = glm::identity<Matrix4>();
        Matrix4 inverseTransposeMatrix = glm::identity<Matrix4>();
    };

    class RenderMesh {
    public:
        RenderMesh()          = default;
        virtual ~RenderMesh() = default;

        SKY_DISABLE_COPY(RenderMesh)

        void SetWorldMatrix(const Matrix4 &matrix);

        const ObjectInfo& GetObjectInfo() const;

        virtual void AddToScene(RenderScene &scene);

        virtual void RemoveFromScene(RenderScene &scene);

        virtual void OnRender(RenderScene &scene);

        virtual void OnGatherRenderPrimitives(RenderView &view)
        {
        }

    protected:
        void               UpdateBuffer();
        ObjectInfo         objectInfo;
        RDDesGroupPtr      objectSet;
        RDDynBufferViewPtr objectBuffer;
    };
} // namespace sky