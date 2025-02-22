//
// Created by blues on 2024/9/14.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/shapes/AABB.h>
#include <render/resource/Buffer.h>
#include <render/resource/Technique.h>
#include <render/RenderBase.h>

namespace sky {

    struct VertexAttribute {
        VertexSemanticFlags sematic = {};
        uint32_t binding            = 0; // buffer index
        uint32_t offset             = 0;
        rhi::Format format          = rhi::Format::F_RGBA32;
        rhi::VertexInputRate rate   = rhi::VertexInputRate::PER_VERTEX;
    };

    struct MeshletGeometry : public RefObject {
        RDBufferPtr posBuffer;
        RDBufferPtr extBuffer;

        RDBufferPtr meshletTriangles;
        RDBufferPtr meshletVertices;
        RDBufferPtr meshlets;
    };
    using MeshletGeometryPtr = CounterPtr<MeshletGeometry>;

    struct RenderGeometry;
    using RenderGeometryPtr = CounterPtr<RenderGeometry>;

    struct RenderGeometry : public RefObject {
        void AddVertexAttribute(const VertexAttribute &attribute);
        void FillVertexBuffer(std::vector<rhi::BufferView> &vbs);
        rhi::VertexAssemblyPtr Request(const RDProgramPtr& program, rhi::VertexInputPtr &vtxDesc);
        rhi::VertexInputPtr    Request(const RDProgramPtr& program);
        void Reset();
        void Upload();
        bool IsReady() const;

        RenderGeometryPtr Duplicate();

        // streams
        std::vector<VertexBuffer>    vertexBuffers;
        std::vector<VertexAttribute> vertexAttributes;
        IndexBuffer                  indexBuffer;
        MeshletGeometryPtr           cluster;
        bool                         dynamicVB = false;

        // flags for all attributes
        VertexSemanticFlags          attributeSemantics;

        uint32_t version = 0;
        bool uploaded = false;
    };

} // namespace sky
