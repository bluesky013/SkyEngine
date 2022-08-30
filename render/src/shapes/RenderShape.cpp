//
// Created by Zach Lee on 2022/7/19.
//

#include <render/shapes/RenderShape.h>

namespace sky {

    namespace impl {
        static Vector3 POS1 = {-1.0f, 0.f, 1.0f};
        static Vector3 POS2 = {-1.0f, 0.f, -1.0f};
        static Vector3 POS3 = {1.0f, 0.f, -1.0f};
        static Vector3 POS4 = {1.0f, 0.f, 1.0f};

        static Vector2 UV1 = {0.f, 1.f};
        static Vector2 UV2 = {0.f, 0.f};
        static Vector2 UV3 = {1.f, 0.f};
        static Vector2 UV4 = {1.f, 1.f};

        inline void AddPlane(const Vector3 &normal, MeshRawData &out)
        {
            Vector3 axis  = glm::cross(normal, Vector3(0, 1.f, 0));
            float   angle = glm::acos(glm::dot(normal, Vector3(0, 1.f, 0.f)));
            auto    rot   = glm::angleAxis(angle, axis);

            Vector3 pos1 = POS1 * rot;
            Vector3 pos2 = POS2 * rot;
            Vector3 pos3 = POS3 * rot;
            Vector3 pos4 = POS4 * rot;

            Vector3 tangent1 = {0.f, 0.f, 0.f};
            Vector3 tangent2 = {0.f, 0.f, 0.f};

            Vector3 edge1    = POS2 - POS1;
            Vector3 edge2    = POS3 - POS1;
            Vector2 deltaUV1 = UV2 - UV1;
            Vector2 deltaUV2 = UV3 - UV1;

            float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

            tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            tangent1   = glm::normalize(tangent1);

            edge1    = POS3 - POS1;
            edge2    = POS4 - POS1;
            deltaUV1 = UV3 - UV1;
            deltaUV2 = UV4 - UV1;

            f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

            tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            tangent2   = glm::normalize(tangent2);

            std::vector<float> positions = {pos1.x, pos1.y, pos1.z, 1.f, pos2.x, pos2.y, pos2.z, 1.f, pos3.x, pos3.y, pos3.z, 1.f,
                                            pos1.x, pos1.y, pos1.z, 1.f, pos3.x, pos3.y, pos3.z, 1.f, pos4.x, pos4.y, pos4.z, 1.f};
            out.positions.insert(out.positions.end(), positions.begin(), positions.end());

            std::vector<float> normals = {
                normal.x, normal.y, normal.z, 1.f, normal.x, normal.y, normal.z, 1.f, normal.x, normal.y, normal.z, 1.f,
                normal.x, normal.y, normal.z, 1.f, normal.x, normal.y, normal.z, 1.f, normal.x, normal.y, normal.z, 1.f,
            };
            out.normals.insert(out.normals.end(), normals.begin(), normals.end());

            std::vector<float> tangents{
                tangent1.x, tangent1.y, tangent1.z, 1.f, tangent1.x, tangent1.y, tangent1.z, 1.f, tangent1.x, tangent1.y, tangent1.z, 1.f,
                tangent2.x, tangent2.y, tangent2.z, 1.f, tangent2.x, tangent2.y, tangent2.z, 1.f, tangent2.x, tangent2.y, tangent2.z, 1.f,
            };
            out.tangents.insert(out.tangents.end(), tangents.begin(), tangents.end());

            std::vector<float> colors = {
                1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,
            };
            out.colors.insert(out.colors.end(), colors.begin(), colors.end());

            std::vector<float> uvs = {
                UV1.x, UV1.y, UV2.x, UV2.y, UV3.x, UV3.y, UV1.x, UV1.y, UV3.x, UV3.y, UV4.x, UV4.y,
            };
            out.uvs.insert(out.uvs.end(), uvs.begin(), uvs.end());
        }
    } // namespace impl

    static RDBufferViewPtr CopyData(RDBufferPtr &buffer, const std::vector<float> &values, uint64_t &offset, uint32_t stride)
    {
        uint64_t size    = values.size() * sizeof(float);
        auto     viewPtr = std::make_shared<BufferView>(buffer, size, offset, stride);
        buffer->Write(reinterpret_cast<const uint8_t *>(values.data()), size, offset);
        offset += size;
        return viewPtr;
    }

    RDMeshPtr RenderShape::CreateMesh(RDMaterialPtr material)
    {
        auto          mesh = std::make_shared<Mesh>();
        Mesh::Builder builder(*mesh);

        SubMesh subMesh  = {};
        subMesh.drawData = drawData;
        subMesh.material = material;
        subMesh.aabb     = aabb;

        for (auto &vb : vertexBuffers) {
            builder.AddVertexBuffer(vb);
        }

        for (auto &desc : vertexDescriptions) {
            builder.AddVertexDesc(desc);
        }

        builder.AddSubMesh(subMesh);

        return mesh;
    }

    void Plane::Init()
    {
        MeshRawData rawData;
        impl::AddPlane(Vector3{0.f, 1.f, 0.f}, rawData);

        Buffer::Descriptor descriptor = {};
        descriptor.size               = rawData.Size();
        descriptor.usage              = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        descriptor.memory             = VMA_MEMORY_USAGE_GPU_ONLY;
        descriptor.allocCPU           = true;

        uint64_t offset = 0;
        buffer          = std::make_shared<Buffer>(descriptor);
        buffer->InitRHI();

        vertexBuffers.emplace_back(CopyData(buffer, rawData.positions, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, rawData.normals, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, rawData.tangents, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, rawData.colors, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, rawData.uvs, offset, 8));
        buffer->Update(true);

        vertexDescriptions.emplace_back(VertexDesc{"inPos", 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{"inNormal", 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{"inTangent", 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{"inColor", 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{"inUv", 4, 0, VK_FORMAT_R32G32_SFLOAT});

        drawData.firstVertex = 0;
        drawData.vertexCount = 6;

        aabb.min = Vector3{-1.0f, 0.f, -1.0f};
        aabb.max = Vector3{1.0f, 0.f, 1.0f};
    }

    void Cube::Init()
    {
        MeshRawData rawData;
        impl::AddPlane(Vector3{1.f, 0.f, 0.f}, rawData);
        impl::AddPlane(Vector3{0.f, 1.f, 0.f}, rawData);
        impl::AddPlane(Vector3{0.f, 0.f, 1.f}, rawData);

        Buffer::Descriptor descriptor = {};
        descriptor.size               = rawData.Size();
        descriptor.usage              = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        descriptor.memory             = VMA_MEMORY_USAGE_GPU_ONLY;
        descriptor.allocCPU           = true;

        uint64_t offset = 0;
        buffer          = std::make_shared<Buffer>(descriptor);
        buffer->InitRHI();

        vertexBuffers.emplace_back(CopyData(buffer, rawData.positions, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, rawData.normals, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, rawData.tangents, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, rawData.colors, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, rawData.uvs, offset, 8));
        buffer->Update(true);

        vertexDescriptions.emplace_back(VertexDesc{"inPos", 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{"inNormal", 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{"inTangent", 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{"inColor", 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{"inUv", 4, 0, VK_FORMAT_R32G32_SFLOAT});

        drawData.firstVertex = 0;
        drawData.vertexCount = 6 * 6;

        aabb.min = Vector3{-1.0f, -1.0f, -1.0f};
        aabb.max = Vector3{1.0f, 1.0f, 1.0f};
    }

} // namespace sky
