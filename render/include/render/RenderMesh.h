//
// Created by Zach Lee on 2022/7/19.
//

#pragma once
#include <core/math/Matrix.h>
#include <core/logger/Logger.h>
#include <core/util/Macros.h>
#include <render/resources/Mesh.h>
#include <render/resources/Material.h>
#include <render/resources/DescirptorGroup.h>

namespace sky {
    class RenderScene;

    struct ObjectInfo {
        Matrix4 worldMatrix;
        Matrix4 inverseTransposeMatrix;
    };

    class RenderPrimitive {
    public:
        RenderPrimitive() = default;
        virtual ~RenderPrimitive() = default;

        inline void SetObjectSet(RDDesGroupPtr value)
        {
            objectSet = value;
        }

        inline void SetMaterial(RDMaterialPtr value)
        {
            material = value;
        }

        inline void SetAABB(const Box& value)
        {
            aabb = value;
        }

        inline void SetViewMask(uint32_t value)
        {
            viewMask |= value;
        }

        inline void SetDrawMask(uint32_t value)
        {
            drawMask |= value;
        }

    protected:
        RDDesGroupPtr objectSet;
        RDMaterialPtr material;
        Box aabb;
        uint32_t viewMask = 0;
        uint32_t drawMask = 0;
    };

    class RenderMesh {
    public:
        RenderMesh() = default;
        virtual ~RenderMesh() = default;

        SKY_DISABLE_COPY(RenderMesh)

        void SetWorldMatrix(const Matrix4& matrix);

        virtual void AddToScene(RenderScene& scene);

        virtual void RemoveFromScene(RenderScene& scene);

        virtual void OnRender(RenderScene& scene);

    private:
        void UpdateBuffer();
        ObjectInfo objectInfo;
        RDDesGroupPtr objectSet;

        // [OPT]: Batch to RenderScene
        RDBufferViewPtr objectBuffer;
    };
}