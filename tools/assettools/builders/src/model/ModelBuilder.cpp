//
// Created by Zach Lee on 2022/8/12.
//

#include <builders/model/ModelBuilder.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <render/resources/Mesh.h>
#include <core/math/Vector.h>
#include <filesystem>
#include <sstream>

namespace sky {

    namespace builder {
        struct Node {
            uint32_t parentIndex = ~(0u);
            uint32_t meshIndex = ~(0u);
        };

        struct Scene {
            std::vector<Node> nodes;
            std::vector<MeshAssetPtr> meshes;
            BufferAssetPtr buffer;
            MeshRawData rawData;
            std::vector<uint32_t> indices;
            std::filesystem::path directory;
        };
    }

    static void ProcessSubMesh(aiMesh *mesh, const aiScene *scene, MeshAssetData& data, builder::Scene& outScene)
    {
        SubMeshAsset subMesh;
        subMesh.aabb.min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z};
        subMesh.aabb.max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z};
        subMesh.drawData.firstVertex = 0;
        subMesh.drawData.vertexCount = mesh->mNumVertices;

        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            auto& pos = mesh->mVertices[i];
            auto& normal = mesh->mNormals[i];
            auto& tangent = mesh->mTangents[i];

            outScene.rawData.positions.emplace_back(pos.x);
            outScene.rawData.positions.emplace_back(pos.y);
            outScene.rawData.positions.emplace_back(pos.z);
            outScene.rawData.positions.emplace_back(1.f);

            outScene.rawData.normals.emplace_back(normal.x);
            outScene.rawData.normals.emplace_back(normal.y);
            outScene.rawData.normals.emplace_back(normal.z);
            outScene.rawData.normals.emplace_back(1.0f);

            outScene.rawData.tangents.emplace_back(tangent.x);
            outScene.rawData.tangents.emplace_back(tangent.y);
            outScene.rawData.tangents.emplace_back(tangent.z);
            outScene.rawData.tangents.emplace_back(1.0f);

            if (mesh->HasVertexColors(0)) {
                auto& color = mesh->mColors[0][i];
                outScene.rawData.colors.emplace_back(color.r);
                outScene.rawData.colors.emplace_back(color.g);
                outScene.rawData.colors.emplace_back(color.b);
                outScene.rawData.colors.emplace_back(color.a);
            } else {
                outScene.rawData.colors.emplace_back(1.f);
                outScene.rawData.colors.emplace_back(1.f);
                outScene.rawData.colors.emplace_back(1.f);
                outScene.rawData.colors.emplace_back(1.f);
            }

            if (mesh->HasTextureCoords(0)) {
                auto& uv = mesh->mTextureCoords[0][i];
                outScene.rawData.uvs.emplace_back(uv.x);
                outScene.rawData.uvs.emplace_back(uv.y);
            } else {
                outScene.rawData.uvs.emplace_back(0.f);
                outScene.rawData.uvs.emplace_back(0.f);
            }
        }

        subMesh.drawData.firstIndex = 0;
        subMesh.drawData.indexCount = mesh->mNumFaces * 3;
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            outScene.indices.emplace_back(face.mIndices[0]);
            outScene.indices.emplace_back(face.mIndices[1]);
            outScene.indices.emplace_back(face.mIndices[2]);
        }

        data.subMeshes.emplace_back(subMesh);
    }

    static void ProcessMesh(aiNode *node, const aiScene *scene, MeshAssetPtr outMesh, builder::Scene& outScene)
    {
        MeshAssetData data;
        data.vertexBuffers.resize(static_cast<uint32_t>(MeshAttributeType::NUM));

        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].buffer = outScene.buffer;
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].offset = outScene.rawData.positions.size() * sizeof(float);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].stride = sizeof(Vector4);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].size   = 0;

        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].buffer = outScene.buffer;
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].offset = outScene.rawData.normals.size() * sizeof(float);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].stride = sizeof(Vector4);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].size   = 0;

        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].buffer = outScene.buffer;
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].offset = outScene.rawData.tangents.size() * sizeof(float);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].stride = sizeof(Vector4);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].size   = 0;

        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].buffer = outScene.buffer;
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].offset = outScene.rawData.colors.size() * sizeof(float);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].stride = sizeof(Vector4);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].size   = 0;

        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].buffer = outScene.buffer;
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].offset = outScene.rawData.uvs.size() * sizeof(float);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].stride = 2 * sizeof(float);
        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].size   = 0;

        data.vertexDescriptions.emplace_back(VertexDesc{"inPos",     0, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        data.vertexDescriptions.emplace_back(VertexDesc{"inNormal",  1, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        data.vertexDescriptions.emplace_back(VertexDesc{"inTangent", 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        data.vertexDescriptions.emplace_back(VertexDesc{"inColor",   3, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        data.vertexDescriptions.emplace_back(VertexDesc{"inUv",      4, 0, VK_FORMAT_R32G32_SFLOAT});

        data.indexBuffer.buffer = outScene.buffer;
        data.indexBuffer.offset = outScene.indices.size() * sizeof(uint32_t);
        data.indexBuffer.stride = sizeof(uint32_t);
        data.indexBuffer.size   = 0;

        data.indexType = VK_INDEX_TYPE_UINT32;

        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            uint32_t vertexNum = mesh->mNumVertices;

            ProcessSubMesh(mesh, scene, data, outScene);

            data.indexBuffer.size += mesh->mNumFaces * 3 * sizeof(uint32_t);
            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].size += vertexNum * sizeof(Vector4);
            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].size   += vertexNum * sizeof(Vector4);
            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].size  += vertexNum * sizeof(Vector4);
            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].size    += vertexNum * sizeof(Vector4);
            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].size      += vertexNum * 2 * sizeof(float);
        }

        outMesh->SetData(std::move(data));
    }

    static void ProcessNode(aiNode *node, const aiScene *scene, uint32_t parent, builder::Scene& outScene)
    {
        uint32_t index = static_cast<uint32_t>(outScene.nodes.size());
        outScene.nodes.emplace_back(builder::Node{});
        auto& current = outScene.nodes.back();
        current.parentIndex = parent;

        if (node->mNumMeshes != 0) {
            current.meshIndex = static_cast<uint32_t>(outScene.meshes.size());
            MeshAssetPtr meshAsset = std::make_shared<Asset<Mesh>>();
            outScene.meshes.emplace_back(meshAsset);
            ProcessMesh(node, scene, meshAsset, outScene);
        }

        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            ProcessNode(node->mChildren[i], scene, index, outScene);
        }
    }

    const std::vector<std::string>& ModelBuilder::GetExtensions() const
    {
        static const std::vector<std::string> extensions = {
            ".fbx", ".gltf", ".glb"
        };
        return extensions;
    }

    template <typename T>
    static void CopyData(uint8_t* ptr, size_t& offset, const std::vector<T>& data, builder::Scene& scene, MeshAttributeType type)
    {
        size_t size = data.size() * sizeof(T);
        memcpy(&ptr[offset], data.data(), size);

        for (auto& mesh : scene.meshes) {
            if (type == MeshAttributeType::NUM) {
                mesh->Data().indexBuffer.offset += offset;
            } else {
                mesh->Data().vertexBuffers[static_cast<uint32_t>(type)].offset += offset;
            }
        }
        offset += size;
    }

    void ModelBuilder::Build(const std::string& projectPath, const std::string& path) const
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            return;
        }

        builder::Scene builderScene;
        builderScene.directory = path;
        builderScene.buffer = std::make_shared<Asset<Buffer>>();
        ProcessNode(scene->mRootNode, scene, -1, builderScene);

        size_t vertexBufferSize = builderScene.rawData.Size();
        size_t indicesSize = builderScene.indices.size() * sizeof(uint32_t);

        BufferAssetData bufferData;
        bufferData.data.resize(vertexBufferSize + indicesSize);
        bufferData.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferData.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        uint8_t* ptr = bufferData.data.data();
        size_t offset = 0;

        CopyData(ptr, offset, builderScene.rawData.positions, builderScene, MeshAttributeType::POSITION);
        CopyData(ptr, offset, builderScene.rawData.normals, builderScene, MeshAttributeType::NORMAL);
        CopyData(ptr, offset, builderScene.rawData.tangents, builderScene, MeshAttributeType::TANGENT);
        CopyData(ptr, offset, builderScene.rawData.colors, builderScene, MeshAttributeType::COLOR);
        CopyData(ptr, offset, builderScene.rawData.uvs, builderScene, MeshAttributeType::UV0);
        CopyData(ptr, offset, builderScene.indices, builderScene, MeshAttributeType::NUM);

//        for (auto& mesh : builderScene.meshes) {
//            mesh->Data().indexBuffer.offset += vertexBufferSize;
//        }

        std::filesystem::path dataPath(projectPath);
        dataPath.append("data").append("models");
        if (!std::filesystem::exists(dataPath)) {
            std::filesystem::create_directories(dataPath);
        }
        std::string fileWithoutExt = builderScene.directory.filename().replace_extension().string();
        std::filesystem::path bufferPath = dataPath;
        bufferPath.append(fileWithoutExt + "_buffer.bin");

        builderScene.buffer->SetData(std::move(bufferData));
        builderScene.buffer->SetUuid(Uuid::CreateWithSeed(Fnv1a32(std::filesystem::relative(bufferPath, projectPath).string().data())));
        builderScene.buffer->SaveToPath(bufferPath.string());

        uint32_t index = 0;
        for (auto& mesh : builderScene.meshes) {
            std::filesystem::path meshPath = dataPath;
            std::stringstream ss;
            ss << fileWithoutExt << "_mesh" << index++ << ".mesh";
            mesh->SaveToPath(meshPath.append(ss.str()).string());
        }
    }

}