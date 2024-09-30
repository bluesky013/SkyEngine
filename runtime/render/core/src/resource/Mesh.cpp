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
        for (auto &attr : attributes) {
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

    void Mesh::Upload()
    {
        for (auto &sub : subMeshes) {
            sub.material->Upload();
        }
        geometry->Upload();
        geometry->version++;
    }

    bool Mesh::IsReady() const
    {
        for (const auto &sub : subMeshes) {
            if (!sub.material->IsReady()) {
                return false;
            }
        }
        return geometry->IsReady();
    }

    void Mesh::SetUploadStream(MeshData&& stream_)
    {
        geometry->vertexBuffers.reserve(stream_.vertexStreams.size());
        for (auto &stream : stream_.vertexStreams) {
            VertexBuffer vb = {};
            vb.buffer = new Buffer();
            vb.buffer->Init(stream.source.size, rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST,
                rhi::MemoryType::GPU_ONLY);
            vb.buffer->SetSourceData(stream.source);

            vb.offset = 0;
            vb.range  = stream.source.size;
            vb.stride = stream.stride;
            geometry->vertexBuffers.emplace_back(vb);
        }

        if (stream_.indexStream.source) {
            geometry->indexBuffer.buffer = new Buffer();
            geometry->indexBuffer.buffer->Init(stream_.indexStream.size, rhi::BufferUsageFlagBit::INDEX | rhi::BufferUsageFlagBit::TRANSFER_DST,
                rhi::MemoryType::GPU_ONLY);
            geometry->indexBuffer.buffer->SetSourceData(stream_.indexStream);

            geometry->indexBuffer.offset = 0;
            geometry->indexBuffer.range  = stream_.indexStream.size;
        }
    }

} // namespace sky