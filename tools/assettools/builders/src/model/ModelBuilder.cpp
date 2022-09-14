//
// Created by Zach Lee on 2022/8/12.
//

#include <builders/model/ModelBuilder.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <render/resources/Mesh.h>
#include <render/resources/Prefab.h>
#include <framework/asset/AssetManager.h>
#include <core/math/Vector3.h>
#include <core/math/Matrix4.h>
#include <core/math/Color.h>
#include <core/logger/Logger.h>
#include <stb_image.h>
#include <filesystem>
#include <sstream>

namespace sky {

    static const char* TAG = "ModelBuilder";

    namespace builder {

        struct Scene {
            std::vector<PrefabAssetNode> nodes;
            MeshRawData rawData;
            std::vector<uint32_t> indices;
            BufferAssetPtr buffer;
            std::vector<MeshAssetPtr> meshes;
            std::unordered_map<uint32_t, MaterialAssetPtr> materials;
            std::unordered_map<std::string, ImageAssetPtr> images;
            std::filesystem::path filePath;
            std::filesystem::path directory;
        };
    }

    template <typename T>
    static void SaveAsset(PrefabData &prefab, const std::shared_ptr<Asset<T>> &asset, const std::string &ext,
                          const std::string &projectPath, const std::string &dataPath, const std::string& fileWithoutExt,
                          uint32_t &index)
    {
        std::filesystem::path outPath = dataPath;
        std::stringstream ss;
        ss << fileWithoutExt << '_' << ext << index++ << '.' << ext;

        outPath.append(ss.str());
        auto relativePath = std::filesystem::relative(outPath, projectPath).make_preferred().string();
        auto uuid = Uuid::CreateWithSeed(Fnv1a32(relativePath.data()));
        asset->SetUuid(uuid);
        AssetManager::Get()->SaveAsset(asset, outPath.string());
        prefab.assetPathMap[uuid] = relativePath;
    }

    static PrefabData ToPrefab(const std::string& projectPath, const std::string& dataPath, const std::string& fileWithoutExt, const builder::Scene& outScene)
    {
        PrefabData data;
        data.nodes = outScene.nodes;

        uint32_t index = 0;
        SaveAsset(data, outScene.buffer, "buffer", projectPath, dataPath, fileWithoutExt, index);
        data.buffers.emplace_back(outScene.buffer->GetUuid());

        index = 0;
        for (const auto& [first, image] : outScene.images) {
            SaveAsset(data, image, "image", projectPath, dataPath, fileWithoutExt, index);
            data.images.emplace_back(image->GetUuid());
        }

        index = 0;
        for (const auto& [first, material] : outScene.materials) {
            SaveAsset(data, material, "mat", projectPath, dataPath, fileWithoutExt, index);
            data.materials.emplace_back(material->GetUuid());
        }

        index = 0;
        for (const auto& mesh : outScene.meshes) {
            SaveAsset(data, mesh, "mesh", projectPath, dataPath, fileWithoutExt, index);
            data.meshes.emplace_back(mesh->GetUuid());
        }
        return data;
    }

    static inline Matrix4 FromAssimp(const aiMatrix4x4& trans)
    {
        Matrix4 res;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                res[i][j] = trans[j][i];
            }
        }
        return res;
    }

    static void ProcessTexture(const aiScene *scene, const aiString& path, builder::Scene& outScene, const std::string &name, MaterialAssetData &data)
    {
        auto iter = outScene.images.find(path.data);
        if (iter != outScene.images.end()) {
            return;
        }

        int width = 0;
        int height = 0;
        int channel = 0;
        ImageAssetData assetData;

        auto modelPath = std::filesystem::path(outScene.directory).append(path.data);
        stbi_uc * srcData = nullptr;
        if (std::filesystem::exists(modelPath)) {
            srcData = stbi_load(modelPath.string().data(), &width, &height, &channel, 4);
        } else {
            const auto *tex = scene->GetEmbeddedTexture(path.data);
            if (tex == nullptr) {
                return;
            }
            if (tex->mHeight != 0) {

            } else {
                const uint32_t size = tex->mWidth;
                srcData = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(tex->pcData), static_cast<int>(size),
                                                 &width, &height, &channel, 4);
            }
        }
        if (srcData == nullptr) {
            return;
        }

        assetData.width = static_cast<uint32_t>(width);
        assetData.height = static_cast<uint32_t>(height);
        assetData.format = VK_FORMAT_R8G8B8A8_SRGB;

        uint64_t dataSize = width * height * 4;
        assetData.data.resize(dataSize);
        memcpy(assetData.data.data(), srcData, dataSize);
        stbi_image_free(srcData);

        auto texAsset = std::make_shared<Asset<Image>>();
        texAsset->SetData(std::move(assetData));
        outScene.images.emplace(path.data, texAsset);
        data.properties.emplace_back(PropertyAssetData{name, MaterialPropertyType::TEXTURE, Any(texAsset.get())});
    }

    static void ProcessMaterial(const aiScene *scene, uint32_t materialIndex, builder::Scene& outScene)
    {
        aiMaterial* material = scene->mMaterials[materialIndex];
        aiShadingMode shadingModel = aiShadingMode_Flat;
        material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

        MaterialAssetPtr materialAsset = std::make_shared<Asset<Material>>();
        outScene.materials.emplace(materialIndex, materialAsset);
        MaterialAssetData data;

        LOG_I(TAG, "shader model %d", shadingModel);
        aiString str;
        bool useMap = false;
        if (!material->Get(AI_MATKEY_USE_AO_MAP, useMap)) {
            data.properties.emplace_back(PropertyAssetData{"useAOMap", MaterialPropertyType::BOOL, Any(useMap)});
        }

        if (!material->Get(AI_MATKEY_USE_EMISSIVE_MAP, useMap)) {
            data.properties.emplace_back(PropertyAssetData{"useEmissiveMap", MaterialPropertyType::BOOL, Any(useMap)});
        }

        if (!material->Get(AI_MATKEY_TEXTURE_NORMALS(0), str)) {
            ProcessTexture(scene, str, outScene, "normalMap", data);
        }

        if (!material->Get(AI_MATKEY_TEXTURE_EMISSIVE(0), str)) {
            ProcessTexture(scene, str, outScene, "emissiveMap", data);
        }

        if (!material->Get(AI_MATKEY_TEXTURE_LIGHTMAP(0), str)) {
            ProcessTexture(scene, str, outScene, "lightMap", data);
        }

        if (shadingModel == aiShadingMode_PBR_BRDF) {

            float scalar;
            Vector4 vec4;
            if (!material->Get(AI_MATKEY_BASE_COLOR, vec4)) {
                data.properties.emplace_back(PropertyAssetData{"factors.baseColor", MaterialPropertyType::VEC4, Any(vec4)});
            }

            if (!material->Get(AI_MATKEY_METALLIC_FACTOR, scalar)) {
                data.properties.emplace_back(PropertyAssetData{"factors.metallic", MaterialPropertyType::FLOAT, Any(scalar)});
            }

            if (!material->Get(AI_MATKEY_ROUGHNESS_FACTOR, scalar)) {
                data.properties.emplace_back(PropertyAssetData{"factors.roughness", MaterialPropertyType::FLOAT, Any(scalar)});
            }

            if (!material->Get(AI_MATKEY_USE_COLOR_MAP, useMap)) {
                data.properties.emplace_back(PropertyAssetData{"useBaseColorMap", MaterialPropertyType::BOOL, Any(useMap)});
            }

            if (!material->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &str)) {
                ProcessTexture(scene, str, outScene, "baseColorMap", data);
                data.properties.emplace_back(PropertyAssetData{"useBaseColorMap", MaterialPropertyType::BOOL, Any(useMap)});
            }

            if (!material->Get(AI_MATKEY_USE_METALLIC_MAP, useMap)) {
                data.properties.emplace_back(PropertyAssetData{"useMetallicMapMap", MaterialPropertyType::BOOL, Any(useMap)});
            }

            if (!material->GetTexture(AI_MATKEY_METALLIC_TEXTURE, &str)) {
                ProcessTexture(scene, str, outScene, "metallicMap", data);
            }

            if (!material->Get(AI_MATKEY_USE_ROUGHNESS_MAP, useMap)) {
                data.properties.emplace_back(PropertyAssetData{"useRoughnessMap", MaterialPropertyType::BOOL, Any(useMap)});
            }

            if (!material->GetTexture(AI_MATKEY_ROUGHNESS_TEXTURE, &str)) {
                ProcessTexture(scene, str, outScene, "roughnessMap", data);
            }

            if (!material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &str)) {
                data.properties.emplace_back(PropertyAssetData{"useMetallicRoughnessMap", MaterialPropertyType::BOOL, Any(true)});
                ProcessTexture(scene, str, outScene, "metallicRoughnessMap", data);
            }
        }
        materialAsset->SetData(std::move(data));
    }

    static void ProcessSubMesh(aiMesh *mesh, const aiScene *scene, MeshAssetData& data, builder::Scene& outScene,
                               uint32_t &vertexOffset, uint32_t &indexOffset)
    {
        SubMeshAsset subMesh;
        subMesh.aabb.min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z};
        subMesh.aabb.max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z};
        subMesh.drawData.firstVertex = vertexOffset;
        subMesh.drawData.vertexCount = mesh->mNumVertices;
        vertexOffset += subMesh.drawData.vertexCount;

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

        subMesh.drawData.firstIndex = indexOffset;
        subMesh.drawData.indexCount = mesh->mNumFaces * 3;
        indexOffset += subMesh.drawData.indexCount;
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            outScene.indices.emplace_back(face.mIndices[0]);
            outScene.indices.emplace_back(face.mIndices[1]);
            outScene.indices.emplace_back(face.mIndices[2]);
        }

        ProcessMaterial(scene, mesh->mMaterialIndex, outScene);

        data.subMeshes.emplace_back(subMesh);
    }

    static void ProcessMesh(aiNode *node, const aiScene *scene, const MeshAssetPtr &outMesh, builder::Scene& outScene)
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

        uint32_t indexOffset = 0;
        uint32_t vertexOffset = 0;
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            uint32_t vertexNum = mesh->mNumVertices;

            ProcessSubMesh(mesh, scene, data, outScene, vertexOffset, indexOffset);

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
        auto index = static_cast<uint32_t>(outScene.nodes.size());
        outScene.nodes.emplace_back(PrefabAssetNode{});
        auto& current = outScene.nodes.back();
        current.parentIndex = parent;

        current.transform = FromAssimp(node->mTransformation);

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

    void ModelBuilder::Build(const std::string& projectPath, const std::filesystem::path& path) const
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path.string(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            return;
        }

        builder::Scene builderScene;
        builderScene.filePath = path;
        builderScene.directory = builderScene.filePath.parent_path();
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
        builderScene.buffer->SetData(std::move(bufferData));

        std::filesystem::path dataPath(projectPath);
        dataPath.append("data").append("models");
        if (!std::filesystem::exists(dataPath)) {
            std::filesystem::create_directories(dataPath);
        }

        std::string fileWithoutExt = builderScene.filePath.filename().replace_extension().string();
        PrefabData outData = ToPrefab(projectPath, dataPath.string(), fileWithoutExt, builderScene);
        auto prefabAsset = std::make_shared<Asset<Prefab>>();
        prefabAsset->SetData(std::move(outData));
        AssetManager::Get()->SaveAsset(prefabAsset, dataPath.append(fileWithoutExt + ".prefab").string());
    }

    ModelBuilder::ModelBuilder()
    {
        AssetManager::Get()->RegisterAssetHandler<Mesh>();
        AssetManager::Get()->RegisterAssetHandler<Buffer>();
        AssetManager::Get()->RegisterAssetHandler<Image>();
        AssetManager::Get()->RegisterAssetHandler<Shader>();
        AssetManager::Get()->RegisterAssetHandler<GraphicsTechnique>();
        AssetManager::Get()->RegisterAssetHandler<Material>();
        AssetManager::Get()->RegisterAssetHandler<Prefab>();
    }

}