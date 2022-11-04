//
// Created by Zach Lee on 2022/5/26.
//

#include <cereal/archives/json.hpp>
#include <framework/asset/AssetManager.h>
#include <render/resources/Mesh.h>

namespace sky {

    Mesh::Builder::Builder(Mesh &m) : mesh(m)
    {
    }

    Mesh::Builder &Mesh::Builder::SetIndexBuffer(const RDBufferViewPtr &buffer, VkIndexType type)
    {
        mesh.indexBuffer = std::move(buffer);
        mesh.indexType   = type;
        return *this;
    }

    Mesh::Builder &Mesh::Builder::AddVertexBuffer(const RDBufferViewPtr &buffer, VkVertexInputRate rate)
    {
        mesh.vertexBuffers.emplace_back(buffer);
        mesh.inputRates.emplace_back(rate);
        return *this;
    }

    Mesh::Builder &Mesh::Builder::AddVertexDesc(const VertexDesc &desc)
    {
        mesh.vertexDescriptions.emplace_back(desc);
        return *this;
    }

    Mesh::Builder &Mesh::Builder::SetVertexDesc(const std::vector<VertexDesc> &desc)
    {
        mesh.vertexDescriptions = desc;
        return *this;
    }

    Mesh::Builder &Mesh::Builder::AddSubMesh(const SubMesh &subMesh)
    {
        mesh.subMeshes.emplace_back(subMesh);
        return *this;
    }

    const SubMesh &Mesh::GetSubMesh(uint32_t index) const
    {
        return subMeshes[index];
    }

    const std::vector<SubMesh> &Mesh::GetSubMeshes() const
    {
        return subMeshes;
    }

    const std::vector<VertexDesc> &Mesh::GetVertexDesc() const
    {
        return vertexDescriptions;
    }

    const std::vector<RDBufferViewPtr> &Mesh::GetVertexBuffers() const
    {
        return vertexBuffers;
    }

    RDBufferViewPtr Mesh::GetIndexBuffer() const
    {
        return indexBuffer;
    }

    VkIndexType Mesh::GetIndexType() const
    {
        return indexType;
    }

    vk::VertexInputPtr Mesh::BuildVertexInput(Shader &shader) const
    {
        auto                     &shaderInputs = shader.GetStageInputs();
        vk::VertexInput::Builder builder;
        builder.Begin();
        for (auto &attribute : vertexDescriptions) {
            auto iter = shaderInputs.find(attribute.name);
            if (iter == shaderInputs.end() || iter->second.format != attribute.format) {
                return {};
            }
            builder.AddAttribute(iter->second.location, attribute.index, attribute.offset, attribute.format);
        }

        for (uint32_t i = 0; i < vertexBuffers.size(); ++i) {
            builder.AddStream(i, vertexBuffers[i]->GetStride(), inputRates[i]);
        }
        return builder.Build();
    }

    vk::CmdDraw Mesh::BuildDrawArgs(uint32_t index) const
    {
        vk::CmdDraw res{};
        auto        &subMesh = subMeshes[index];
        if (indexBuffer && indexBuffer->IsValid()) {
            res.type                  = vk::CmdDrawType::INDEXED;
            res.indexed.firstInstance = 0;
            res.indexed.instanceCount = 1;
            res.indexed.firstIndex    = subMesh.drawData.firstIndex;
            res.indexed.indexCount    = subMesh.drawData.indexCount;
            res.indexed.vertexOffset  = subMesh.drawData.firstVertex;
        } else {
            res.type                 = vk::CmdDrawType::LINEAR;
            res.linear.firstInstance = 0;
            res.linear.instanceCount = 1;
            res.linear.firstVertex   = subMesh.drawData.firstVertex;
            res.linear.vertexCount   = subMesh.drawData.vertexCount;
        }
        return res;
    }

    uint32_t Mesh::GetSubMeshCount() const
    {
        return static_cast<uint32_t>(subMeshes.size());
    }

    void Mesh::SetMaterial(RDMaterialPtr material, uint32_t index)
    {
        subMeshes[index].material = material;
    }

    SubMesh SubMeshAsset::ToSubMesh() const
    {
        return SubMesh{drawData, aabb, material->CreateInstance()};
    }

    void BufferAssetView::InitBuffer(const Uuid &id)
    {
        buffer = AssetManager::Get()->LoadAsset<Buffer>(id);
    }

    RDBufferViewPtr BufferAssetView::CreateBufferView() const
    {
        return std::make_shared<BufferView>(buffer->CreateInstance(), size, offset, stride);
    }

    RDMeshPtr Mesh::CreateFromData(const MeshAssetData &data)
    {
        RDMeshPtr     mesh = std::make_shared<Mesh>();
        Mesh::Builder builder(*mesh);
        builder.SetVertexDesc(data.vertexDescriptions);
        builder.SetIndexBuffer(data.indexBuffer.CreateBufferView(), data.indexType);
        for (auto &vb : data.vertexBuffers) {
            builder.AddVertexBuffer(vb.CreateBufferView());
        }
        for (auto &subMesh : data.subMeshes) {
            if (subMesh.material) {
                builder.AddSubMesh(subMesh.ToSubMesh());
            }
        }
        return mesh;
    }

    void SubMeshAsset::InitMaterial(const Uuid &id)
    {
        material = AssetManager::Get()->LoadAsset<Material>(id);
    }
} // namespace sky
