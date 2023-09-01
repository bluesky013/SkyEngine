//
// Created by Zach Lee on 2023/2/20.
//

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <builder/render/ImageBuilder.h>
#include <builder/render/PrefabBuilder.h>
#include <builder/render/TechniqueBuilder.h>
#include <core/logger/Logger.h>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/RenderPrefab.h>
#include <sstream>

static const char* TAG = "PrefabBuilder";

namespace sky::builder {

    enum class MeshAttributeType : uint32_t {
        POSITION = 0,
        NORMAL,
        TANGENT,
        BITANGENT,
        COLOR,
        UV,
        NUM,
    };

    struct MeshVertexData {
        std::vector<Vector4> position;
        std::vector<Vector4> normal;
        std::vector<Vector4> tangent;
        std::vector<Vector4> biTangent;
        std::vector<Vector4> color;
        std::vector<Vector4> UV;
    };

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

    static ImageAssetPtr ProcessTexture(const aiScene *scene, const aiString& str, RenderPrefabAssetData &outScene, const std::string &sourceFolder, const std::string &productFolder)
    {
        auto *am = AssetManager::Get();
        auto texPath = std::filesystem::path(sourceFolder).append(str.C_Str());
        if (std::filesystem::exists(texPath)) {
            Uuid texId;
            if (am->QueryOrImportSource(texPath.make_preferred().string(), {ImageBuilder::KEY.data(), productFolder, false}, texId)) {
                return am->LoadAsset<Texture>(texId);
            }
        } else {
//            const auto *tex = scene->GetEmbeddedTexture(path.data);
//            if (tex == nullptr) {
//                return;
//            }
//            if (tex->mHeight != 0) {
//
//            } else {
//                const uint32_t size = tex->mWidth;
//                srcData = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(tex->pcData), static_cast<int>(size),
//                                                            &width, &height, &channel, 4);
//            }
        }
        return {};
    }

    static MaterialAssetPtr CreateAssetStandardPBRMaterial(const std::string &fullPath)
    {
        auto *am = AssetManager::Get();
        auto asset = am->CreateAsset<Material>(fullPath);
        std::string techPath = am->GetRealPath("techniques/standard_forward.tech");
        Uuid techId;
        if (am->QueryOrImportSource(techPath, {TechniqueBuilder::KEY.data()}, techId)) {
            asset->Data().techniques.emplace_back(am->LoadAsset<Technique>(techId));
        }
        return asset;
    }

    static void ProcessMaterials(const aiScene *scene, RenderPrefabAssetData &outScene, const std::filesystem::path &source, const std::string &productFolder)
    {
        uint32_t matSize = scene->mNumMaterials;
        for (uint32_t i = 0; i < matSize; ++i) {
            aiMaterial* material = scene->mMaterials[i];
            aiShadingMode shadingModel = aiShadingMode_Flat;
            material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

            LOG_I(TAG, "shader model %d", shadingModel);
            std::string sourceFolder = source.parent_path().string();

            std::stringstream ss;
            std::string matName = material->GetName().C_Str();

            std::filesystem::path productMatPath(productFolder);
            productMatPath.append(source.filename().replace_extension().string());

            ss << productMatPath.make_preferred().string() << "_mat_";
            if (matName.empty()) {
                ss << i;
            } else {
                ss << matName;
            }
            ss << ".mat";

            auto matAsset = CreateAssetStandardPBRMaterial(ss.str());
            auto &data = matAsset->Data();

            aiString str;
            bool useAOMap = false;
            bool useEmissiveMap = false;
            bool useBaseColorMap = false;
            bool useNormalMap = false;
            bool useMetallicRoughnessMap = false;
            bool useMask = false;
            Vector4 baseColor = Vector4(1.f, 1.f, 1.f, 1.f);
            float metallic = 1.f;
            float roughness = 1.f;
            ImageAssetPtr normalMap;
            ImageAssetPtr emissiveMap;
            ImageAssetPtr aoMap;
            ImageAssetPtr baseColorMap;
            ImageAssetPtr metallicRoughnessMap;

            aiString aiAlphaMode;
            if (!material->Get(AI_MATKEY_GLTF_ALPHAMODE, aiAlphaMode)) {
                std::string mode = aiAlphaMode.data;
                if (mode == "MASK") {
                    useMask = true;
                }
            }

            float alphaCutoff = 0.5f;
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);

            if (!material->Get(AI_MATKEY_TEXTURE_NORMALS(0), str)) {
                // normalMap
                normalMap = ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
                useNormalMap = !!normalMap;
            }

            if (!material->Get(AI_MATKEY_TEXTURE_EMISSIVE(0), str)) {
                // emissiveMap
                emissiveMap = ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
                useEmissiveMap = !!emissiveMap;
            }

            if (!material->Get(AI_MATKEY_TEXTURE_LIGHTMAP(0), str)) {
                // aoMap;
                aoMap = ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
                useAOMap = !!aoMap;
            }

            if (shadingModel == aiShadingMode_PBR_BRDF) {
                material->Get(AI_MATKEY_BASE_COLOR, baseColor);
                material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
                material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);


                if (!material->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &str)) {
                    // baseColorMap
                    baseColorMap = ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
                    useBaseColorMap = !!baseColorMap;
                }

                if (!material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &str)) {
                    // metallicRoughnessMap
                    metallicRoughnessMap = ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
                    useMetallicRoughnessMap = !!metallicRoughnessMap;
                }
            }

            data.valueMap.emplace("useAOMap", Any(useAOMap));
            if (useAOMap) {
                data.valueMap.emplace("aOMap", Any(MaterialTexture{static_cast<uint32_t>(data.images.size())}));
                data.images.emplace_back(aoMap);
            }

            data.valueMap.emplace("useEmissiveMap", Any(useEmissiveMap));
            if (useEmissiveMap) {
                data.valueMap.emplace("emissiveMap", Any(MaterialTexture{static_cast<uint32_t>(data.images.size())}));
                data.images.emplace_back(emissiveMap);
            }

            data.valueMap.emplace("useBaseColorMap", Any(useBaseColorMap));
            if (useBaseColorMap) {
                data.valueMap.emplace("baseColorMap", Any(MaterialTexture{static_cast<uint32_t>(data.images.size())}));
                data.images.emplace_back(baseColorMap);
            }

            data.valueMap.emplace("useMetallicRoughnessMap", Any(useMetallicRoughnessMap));
            if (useMetallicRoughnessMap) {
                data.valueMap.emplace("metallicRoughnessMap", Any(MaterialTexture{static_cast<uint32_t>(data.images.size())}));
                data.images.emplace_back(metallicRoughnessMap);
            }

            data.valueMap.emplace("useNormalMap", Any(useNormalMap));
            if (useNormalMap) {
                data.valueMap.emplace("normalMap", Any(MaterialTexture{static_cast<uint32_t>(data.images.size())}));
                data.images.emplace_back(normalMap);
            }

            data.valueMap.emplace("useMask",     Any(useMask));

            data.valueMap.emplace("baseColor",   Any(baseColor));
            data.valueMap.emplace("metallic",    Any(metallic));
            data.valueMap.emplace("roughness",   Any(roughness));
            data.valueMap.emplace("alphaCutoff", Any(alphaCutoff));
            AssetManager::Get()->SaveAsset(matAsset);

            outScene.materials.emplace_back(matAsset);
        }
    }

    static void ProcessSubMesh(aiMesh *mesh, const aiScene *scene, RenderPrefabAssetData& outScene, MeshAssetData &meshData, uint32_t &vertexOffset, uint32_t &indexOffset)
    {
        uint32_t vertexNum   = mesh->mNumVertices;
        uint32_t currentSize = vertexOffset + vertexNum;

        meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].resize(currentSize * sizeof(Vector4));
        meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].resize(currentSize * sizeof(Vector4));
        meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].resize(currentSize * sizeof(Vector4));
        meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::BITANGENT)].resize(currentSize * sizeof(Vector4));
        meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].resize(currentSize * sizeof(Vector4));
        meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV)].resize(currentSize * sizeof(Vector4));

        Vector4 *position = &reinterpret_cast<Vector4 *>(meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].data())[vertexOffset];
        Vector4 *normal = &reinterpret_cast<Vector4 *>(meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].data())[vertexOffset];
        Vector4 *tangent = &reinterpret_cast<Vector4 *>(meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].data())[vertexOffset];
        Vector4 *biTangent = &reinterpret_cast<Vector4 *>(meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::BITANGENT)].data())[vertexOffset];
        Vector4 *color = &reinterpret_cast<Vector4 *>(meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].data())[vertexOffset];
        Vector4 *uv = &reinterpret_cast<Vector4 *>(meshData.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV)].data())[vertexOffset];

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            auto &p = mesh->mVertices[i];
            auto &n = mesh->mNormals[i];
            auto &t = mesh->mTangents[i];
            auto &b = mesh->mBitangents[i];

            position->x = p.x;
            position->y = p.y;
            position->z = p.z;
            position->x = 1.f;

            normal->x = n.x;
            normal->y = n.y;
            normal->z = n.z;
            normal->x = 1.f;

            tangent->x = t.x;
            tangent->y = t.y;
            tangent->z = t.z;
            tangent->x = 1.f;

            biTangent->x = b.x;
            biTangent->y = b.y;
            biTangent->z = b.z;
            biTangent->x = 1.f;

            if (mesh->HasVertexColors(0)) {
                auto &c  = mesh->mColors[0][i];
                color->x = c.r;
                color->y = c.g;
                color->z = c.b;
                color->w = c.a;
            } else {
                color->x = 1.f;
                color->y = 1.f;
                color->z = 1.f;
                color->w = 1.f;
            }

            if (mesh->HasTextureCoords(0)) {
                auto &u = mesh->mTextureCoords[0][i];
                uv->x   = u.x;
                uv->y   = u.y;
            } else {
                uv->x = 0.f;
                uv->y = 0.f;
            }
        }

        SubMeshAssetData subMesh;
        subMesh.aabb.min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z};
        subMesh.aabb.max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z};
        subMesh.firstVertex = vertexOffset;
        subMesh.vertexCount = mesh->mNumVertices;
        subMesh.firstIndex = indexOffset;
        subMesh.indexCount = mesh->mNumFaces * 3;
        subMesh.material = outScene.materials[mesh->mMaterialIndex];

        uint32_t currentIndexSize = indexOffset + subMesh.indexCount;
        meshData.indexBuffer.resize(currentIndexSize * sizeof(uint32_t));
        uint32_t *indices = &reinterpret_cast<uint32_t *>(meshData.indexBuffer.data())[indexOffset];
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            indices[i + 0] = face.mIndices[0];
            indices[i + 1] = face.mIndices[1];
            indices[i + 2] = face.mIndices[2];
        }

        meshData.subMeshes.emplace_back(subMesh);

        indexOffset += subMesh.indexCount;
        vertexOffset += subMesh.vertexCount;
    }

    static void ProcessMesh(const aiScene *scene, const aiNode *node, RenderPrefabAssetData& outScene, MeshAssetData &meshData)
    {
        meshData.vertexDescriptions.emplace_back(VertexDesc{"POSITION", 0, 0, rhi::Format::F_RGBA32});
        meshData.vertexDescriptions.emplace_back(VertexDesc{"NORMAL", 1, 0, rhi::Format::F_RGBA32});
        meshData.vertexDescriptions.emplace_back(VertexDesc{"TANGENT", 2, 0, rhi::Format::F_RGBA32});
        meshData.vertexDescriptions.emplace_back(VertexDesc{"BITANGENT", 3, 0, rhi::Format::F_RGBA32});
        meshData.vertexDescriptions.emplace_back(VertexDesc{"COLOR", 4, 0, rhi::Format::F_RGBA32});
        meshData.vertexDescriptions.emplace_back(VertexDesc{"UV", 5, 0, rhi::Format::F_RGBA32});

        meshData.vertexBuffers.resize(static_cast<uint32_t>(MeshAttributeType::NUM));

        uint32_t meshNum = node->mNumMeshes;
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        for (uint32_t i = 0; i < meshNum; i++) {
            aiMesh  *mesh      = scene->mMeshes[node->mMeshes[i]];
            ProcessSubMesh(mesh, scene, outScene, meshData, vertexOffset, indexOffset);
        }
    }

    static void ProcessNode(aiNode *node, const aiScene *scene, uint32_t parent, RenderPrefabAssetData& outScene, const std::filesystem::path &source, const std::string &productFolder)
    {
        auto *am = AssetManager::Get();
        auto index = static_cast<uint32_t>(outScene.nodes.size());
        outScene.nodes.emplace_back(RenderPrefabNode{});
        auto& current = outScene.nodes.back();
        current.parentIndex = parent;
//        if (parent != ~(0U)) {
//            outScene.nodes[parent].children.emplace_back(index);
//        }
        current.localMatrix = FromAssimp(node->mTransformation);

        if (node->mNumMeshes != 0) {
            current.meshIndex = static_cast<uint32_t>(outScene.meshes.size());

            std::filesystem::path productMatPath(productFolder);
            productMatPath.append(source.filename().replace_extension().string());

            std::stringstream ss;
            ss << productMatPath.make_preferred().string() << "_mesh_" << current.meshIndex << ".mesh";
            auto meshAsset = am->CreateAsset<Mesh>(ss.str());
            MeshAssetData &meshAssetData = meshAsset->Data();
            ProcessMesh(scene, node, outScene, meshAssetData);

            outScene.meshes.emplace_back(meshAsset);
            am->SaveAsset(meshAsset);
        }

        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            ProcessNode(node->mChildren[i], scene, index, outScene, source, productFolder);
        }
    }


    void PrefabBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        Assimp::Importer importer;
        if (!std::filesystem::exists(request.fullPath)) {
            return;
        }

        const aiScene* scene = importer.ReadFile(request.fullPath.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            return;
        }
        AssetManager *am = AssetManager::Get();
        std::filesystem::path fullPath(request.fullPath);
        fullPath.make_preferred();

        std::filesystem::path outPath(request.outDir);
        std::string outFolder = outPath.append(request.name).replace_extension().string();
        std::filesystem::create_directories(outPath);
        outPath.append(request.name);

        auto asset = am->CreateAsset<RenderPrefab>(outPath.make_preferred().replace_extension().string() + ".prefab");
        auto &assetData = asset->Data();

        ProcessMaterials(scene, assetData, fullPath, outFolder);
        ProcessNode(scene->mRootNode, scene, -1, assetData, fullPath, outFolder);

        result.products.emplace_back(BuildProduct{KEY.data(), asset->GetUuid()});
        am->SaveAsset(asset);
    }

} // namespace sky::builder
