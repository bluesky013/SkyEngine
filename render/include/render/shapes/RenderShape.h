//
// Created by Zach Lee on 2022/7/19.
//

#pragma once

#include <render/resources/Mesh.h>

namespace sky {

    class RenderShape {
    public:
        RenderShape()          = default;
        virtual ~RenderShape() = default;

        virtual void Init() = 0;

        RDMeshPtr CreateMesh(RDMaterialPtr material);

    protected:
        RDBufferViewPtr              indexBuffer;
        SubMeshDrawData              drawData;
        Box                          aabb;
        std::vector<RDBufferViewPtr> vertexBuffers;
        std::vector<VertexDesc>      vertexDescriptions;
    };
    using RDShaperPtr = std::shared_ptr<RenderShape>;

    class Plane : public RenderShape {
    public:
        Plane()  = default;
        ~Plane() = default;

        void Init() override;

    private:
        RDBufferPtr buffer;
    };

    class Cube : public RenderShape {
    public:
        Cube()  = default;
        ~Cube() = default;

        void Init() override;

    private:
        RDBufferPtr buffer;
    };

} // namespace sky