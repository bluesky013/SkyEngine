//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Mesh.h>
#include <render/RHI.h>
#include <render/Renderer.h>

namespace sky {

    Mesh::Mesh()
        : geometry(new RenderGeometry())
    {
    }

    void Mesh::AddSubMesh(const SubMesh &sub)
    {
        subMeshes.emplace_back(sub);
    }

    void Mesh::SetVertexAttributes(const std::vector<VertexAttribute> &attributes)
    {
        geometry->vertexAttributes = attributes;
        for (const auto &attr : attributes) {
            geometry->attributeSemantics |= attr.sematic;
        }
    }

    void Mesh::SetIndexType(rhi::IndexType type)
    {
        geometry->indexBuffer.indexType = type;
    }

    void Mesh::SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh)
    {
        subMeshes[subMesh].material = mat;
    }

    void Mesh::SetUploadStream(MeshUploadData&& stream_)
    {
        meshData = std::move(stream_);

        geometry->vertexBuffers.reserve(meshData.vertexStreams.size());

        rhi::BufferUsageFlags externalFlag = rhi::BufferUsageFlagBit::NONE;
        if (meshData.meshlets.source && meshData.meshletVertices.source && meshData.meshletTriangles.source) {
            geometry->cluster = new MeshletGeometry();
            externalFlag |= rhi::BufferUsageFlagBit::STORAGE;

            geometry->cluster->meshlets = new Buffer();
            geometry->cluster->meshlets->Init(meshData.meshlets.size, externalFlag | rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
            geometry->cluster->meshlets->SetSourceData(meshData.meshlets);

            geometry->cluster->meshletTriangles = new Buffer();
            geometry->cluster->meshletTriangles->Init(meshData.meshletTriangles.size, externalFlag | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
            geometry->cluster->meshletTriangles->SetSourceData(meshData.meshletTriangles);

            geometry->cluster->meshletVertices = new Buffer();
            geometry->cluster->meshletVertices->Init(meshData.meshletVertices.size, externalFlag | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
            geometry->cluster->meshletVertices->SetSourceData(meshData.meshletVertices);
        }

        for (auto &stream : meshData.vertexStreams) {
            VertexBuffer vb = {};
            vb.buffer = new Buffer();
            vb.buffer->Init(stream.source.size, externalFlag | rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST,
                rhi::MemoryType::GPU_ONLY);
            vb.buffer->SetSourceData(stream.source);

            vb.offset = 0;
            vb.range  = stream.source.size;
            vb.stride = stream.stride;
            geometry->vertexBuffers.emplace_back(vb);
        }

        if (meshData.indexStream.source) {
            geometry->indexBuffer.buffer = new Buffer();
            geometry->indexBuffer.buffer->Init(meshData.indexStream.size, externalFlag | rhi::BufferUsageFlagBit::INDEX | rhi::BufferUsageFlagBit::TRANSFER_DST,
                rhi::MemoryType::GPU_ONLY);
            geometry->indexBuffer.buffer->SetSourceData(meshData.indexStream);

            geometry->indexBuffer.offset = 0;
            geometry->indexBuffer.range  = meshData.indexStream.size;
        }

        if (geometry->cluster) {
            VertexSemanticFlags flags = {};

            uint32_t positionBinding = INVALID_INDEX;
            uint32_t extBinding = INVALID_INDEX;

            for (auto &attr : geometry->vertexAttributes) {
                if (attr.sematic & VertexSemanticFlagBit::POSITION) {
                    positionBinding = attr.binding;

                    flags |= attr.sematic;
                } else if (attr.sematic & VertexSemanticFlagBit::STANDARD_ATTR) {
                    if (extBinding != INVALID_INDEX && extBinding != attr.binding) {
                        break;
                    }
                    extBinding = attr.binding;
                    flags |= attr.sematic;
                }
            }
            SKY_ASSERT(positionBinding != extBinding);
            geometry->cluster->posBuffer = geometry->vertexBuffers[positionBinding].buffer;
            geometry->cluster->extBuffer = geometry->vertexBuffers[extBinding].buffer;

            clusterValid =  (flags & VertexSemanticFlagBit::STANDARD_ATTR) == VertexSemanticFlagBit::STANDARD_ATTR;
        }
    }

} // namespace sky