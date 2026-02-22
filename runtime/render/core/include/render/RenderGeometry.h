//
// Created by blues on 2024/9/14.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/shapes/Bounds.h>
#include <render/resource/Buffer.h>
#include <render/resource/Technique.h>
#include <render/RenderBase.h>

namespace sky {

    struct VertexAttribute {
        VertexSemanticFlags sematic = {};
        uint32_t binding            = 0; // buffer index
        uint32_t offset             = 0;
        rhi::Format format          = rhi::Format::F_RGBA32;
    };

    struct VertexStream {
        uint32_t stride;
        rhi::VertexInputRate rate   = rhi::VertexInputRate::PER_VERTEX;
    };

    struct MeshletGeometry : RefObject {
        RDBufferPtr posBuffer;
        RDBufferPtr extBuffer;

        RDBufferPtr meshletTriangles;
        RDBufferPtr meshletVertices;
        RDBufferPtr meshlets;
    };
    using MeshletGeometryPtr = CounterPtr<MeshletGeometry>;

    struct RenderGeometry;
    using RenderGeometryPtr = CounterPtr<RenderGeometry>;

    struct RenderGeometry : RefObject {
        void AddVertexAttribute(const VertexAttribute &attribute);
        void FillVertexBuffer(std::vector<rhi::BufferView> &vbs);
        rhi::VertexAssemblyPtr Request(const RDProgramPtr& program, rhi::VertexInputPtr &vtxDesc) const;
        rhi::VertexInputPtr    Request(const RDProgramPtr& program) const;
        void Reset();
        void Upload();
        bool IsReady() const;

        RenderGeometryPtr Duplicate();

        // streams
        std::vector<VertexBuffer>    vertexBuffers;
        std::vector<VertexAttribute> vertexAttributes;
        IndexBuffer                  indexBuffer;
        BoundingBoxSphere            localBounds;

        bool                         dynamicVB = false;

        // flags for all attributes
        VertexSemanticFlags          attributeSemantics;
        bool uploaded = false;
    };

    template <typename T>
    struct TLinearGeometry : RenderGeometry {
        TLinearGeometry()
        {
            dynamicVB = true;
        }

        void UpdateData(const std::vector<T>& vertices)
        {
            auto dataSize = static_cast<uint32_t>(vertices.size() * sizeof(T));
            if (!vertexBuffer || dataSize > vertexBuffer->GetSize()) {
                vertexBuffer = new DynamicBuffer();
                vertexBuffer->Init(dataSize, rhi::BufferUsageFlagBit::VERTEX);

                vertexBuffers.clear();
                vertexBuffers.emplace_back(VertexBuffer{
                    vertexBuffer, 0, dataSize, sizeof(T)
                });
            }
            vertexBuffer->SwapBuffer();
            vertexBuffer->Update(reinterpret_cast<const uint8_t *>(vertices.data()), 0, dataSize);
        }
        RDDynamicBuffer vertexBuffer;
    };

} // namespace sky
