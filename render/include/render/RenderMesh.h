//
// Created by Zach Lee on 2022/7/19.
//

#pragma once
#include <core/math/Matrix.h>
#include <core/logger/Logger.h>
#include <render/resources/DescirptorGroup.h>

namespace sky {
    class RenderScene;

    struct ObjectInfo {
        Matrix4 worldMatrix;
        Matrix4 inverseTransposeMatrix;
    };

    class RenderMesh {
    public:
        RenderMesh() = default;
        virtual ~RenderMesh() = default;

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