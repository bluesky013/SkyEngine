//
// Created by Zach Lee on 2022/5/26.
//

#include <render/resources/Mesh.h>
#include <framework/asset/AssetManager.h>
#include <cereal/archives/json.hpp>

namespace sky {

    Mesh::Builder::Builder(Mesh& m) : mesh(m)
    {
    }

    Mesh::Builder& Mesh::Builder::SetIndexBuffer(const RDBufferViewPtr& buffer, VkIndexType type)
    {
        mesh.indexBuffer = std::move(buffer);
        mesh.indexType = type;
        return *this;
    }

    Mesh::Builder& Mesh::Builder::AddVertexBuffer(const RDBufferViewPtr& buffer, VkVertexInputRate rate)
    {
        mesh.vertexBuffers.emplace_back(buffer);
        mesh.inputRates.emplace_back(rate);
        return *this;
    }

    Mesh::Builder& Mesh::Builder::AddVertexDesc(const VertexDesc& desc)
    {
        mesh.vertexDescriptions.emplace_back(desc);
        return *this;
    }

    Mesh::Builder& Mesh::Builder::AddSubMesh(const SubMesh& subMesh)
    {
        mesh.subMeshes.emplace_back(subMesh);
        return *this;
    }

    const SubMesh& Mesh::GetSubMesh(uint32_t index) const
    {
        return subMeshes[index];
    }

    const std::vector<SubMesh>& Mesh::GetSubMeshes() const
    {
        return subMeshes;
    }

    const std::vector<VertexDesc>& Mesh::GetVertexDesc() const
    {
        return vertexDescriptions;
    }

    const std::vector<RDBufferViewPtr>& Mesh::GetVertexBuffers() const
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

    drv::VertexInputPtr Mesh::BuildVertexInput(Shader& shader) const
    {
        auto& shaderInputs = shader.GetStageInputs();
        drv::VertexInput::Builder builder;
        builder.Begin();
        for (auto& attribute : vertexDescriptions) {
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

    drv::CmdDraw Mesh::BuildDrawArgs(uint32_t index) const
    {
        drv::CmdDraw res {};
        auto& subMesh = subMeshes[index];
        if (indexBuffer && indexBuffer->IsValid()) {
            res.type = drv::CmdDrawType::INDEXED;
            res.indexed.firstInstance = 0;
            res.indexed.instanceCount = 1;
            res.indexed.firstIndex = subMesh.drawData.firstIndex;
            res.indexed.indexCount = subMesh.drawData.indexCount;
            res.indexed.vertexOffset = subMesh.drawData.firstVertex;
        } else {
            res.type = drv::CmdDrawType::LINEAR;
            res.linear.firstInstance = 0;
            res.linear.instanceCount = 1;
            res.linear.firstVertex = subMesh.drawData.firstVertex;
            res.linear.vertexCount = subMesh.drawData.vertexCount;
        }
        return res;
    }

    namespace impl {
        void LoadFromPath(const std::string& path, MeshAssetData& data)
        {
            auto realPath = AssetManager::Get()->GetRealPath(path);
            std::ifstream file(realPath,  std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            cereal::JSONInputArchive archive(file);
            archive >> data;
        }

        void SaveToPath(const std::string& path, const MeshAssetData& data)
        {
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            cereal::JSONOutputArchive binOutput(file);
            binOutput << data;
        }

        Mesh* CreateFromData(const MeshAssetData& data)
        {
            return nullptr;
        }
    }

    void BufferAssetView::InitBuffer(const Uuid& id)
    {
        buffer = AssetManager::Get()->LoadAsset<Buffer>(id);
    }
}
