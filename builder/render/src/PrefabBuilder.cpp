//
// Created by Zach Lee on 2023/2/20.
//

#include <builder/render/PrefabBuilder.h>
#include <builder/render/ImageBuilder.h>
#include <builder/render/TechniqueBuilder.h>
#include <framework/asset/AssetManager.h>
#include <render/assets/RenderPrefab.h>
#include <render/assets/Image.h>
#include <render/assets/Material.h>
#include <render/assets/Mesh.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <filesystem>
#include <sstream>
#include <core/logger/Logger.h>

static const char* TAG = "PrefabBuilder";

namespace sky::builder {

    enum class MeshAttributeType : uint32_t {
        POSITION = 0,
        NORMAL,
        TANGENT,
        BITANGENT,
        COLOR,
        UV0,
        NUM,
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
            if (am->QueryOrImportSource(texPath.make_preferred().string(), ImageBuilder::KEY, texId)) {
                return am->LoadAsset<Image>(texId);
            }
        } else {

        }
        return {};
//        int width = 0;
//        int height = 0;
//        int channel = 0;
//        ImageAssetData assetData;
//
//        auto modelPath = std::filesystem::path(outScene.directory).append(texture->mFilename);
//        stbi_uc * srcData = nullptr;
//        if (std::filesystem::exists(modelPath)) {
//            srcData = stbi_load(modelPath.string().data(), &width, &height, &channel, 4);
//        } else {
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
//        }
//        if (srcData == nullptr) {
//            return;
//        }
//
//        assetData.width = static_cast<uint32_t>(width);
//        assetData.height = static_cast<uint32_t>(height);
//        assetData.format = VK_FORMAT_R8G8B8A8_SRGB;
//
//        uint64_t dataSize = width * height * 4;
//        assetData.data.resize(dataSize);
//        memcpy(assetData.data.data(), srcData, dataSize);
//        stbi_image_free(srcData);
//
//        auto texAsset = std::make_shared<Asset<Image>>();
//        texAsset->SetData(std::move(assetData));
//        outScene.images.emplace(path.data, texAsset);
//        data.properties.emplace_back(PropertyAssetData{name, MaterialPropertyType::TEXTURE, Any(texAsset.get())});
    }

    static MaterialAssetPtr CreateAssetStandardPBRMaterial(const std::string &fullPath)
    {
        auto *am = AssetManager::Get();
        auto asset = am->CreateAsset<Material>(fullPath);
        std::string techPath = am->GetRealPath("techniques/standard_forward.tech");
        Uuid techId;
        if (am->QueryOrImportSource(techPath, TechniqueBuilder::KEY, techId)) {
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
                useNormalMap = true;
                // normalMap
                ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
            }

            if (!material->Get(AI_MATKEY_TEXTURE_EMISSIVE(0), str)) {
                useEmissiveMap = true;
                // emissiveMap
                ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
            }

            if (!material->Get(AI_MATKEY_TEXTURE_LIGHTMAP(0), str)) {
                useAOMap = true;
                // aoMap;
                ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
            }

            if (shadingModel == aiShadingMode_PBR_BRDF) {
                material->Get(AI_MATKEY_BASE_COLOR, baseColor);
                material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
                material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);


                if (!material->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &str)) {
                    useBaseColorMap = true;
                    // baseColorMap
                    ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
                }

                if (!material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &str)) {
                    useMetallicRoughnessMap = true;
                    // metallicRoughnessMap
                    ProcessTexture(scene, str, outScene, sourceFolder, productFolder);
                }
            }

            data.valueMap.emplace("useAOMap",                Any(useAOMap));
            data.valueMap.emplace("useEmissiveMap",          Any(useEmissiveMap));
            data.valueMap.emplace("useBaseColorMap",         Any(useBaseColorMap));
            data.valueMap.emplace("useMetallicRoughnessMap", Any(useMetallicRoughnessMap));
            data.valueMap.emplace("useNormalMap",            Any(useNormalMap));
            data.valueMap.emplace("useMask",                 Any(useMask));

            data.valueMap.emplace("baseColor",   Any(baseColor));
            data.valueMap.emplace("metallic",    Any(metallic));
            data.valueMap.emplace("roughness",   Any(roughness));
            data.valueMap.emplace("alphaCutoff", Any(alphaCutoff));
            AssetManager::Get()->SaveAsset(matAsset);
        }
    }

    static void ProcessMeshes(const aiScene *scene, RenderPrefabAssetData& outScene)
    {
//        MeshAssetData data;
//        data.vertexBuffers.resize(static_cast<uint32_t>(MeshAttributeType::NUM));
//
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].buffer = outScene.buffer;
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].offset = outScene.rawData.positions.size() * sizeof(float);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].stride = sizeof(Vector4);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].size   = 0;
//
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].buffer = outScene.buffer;
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].offset = outScene.rawData.normals.size() * sizeof(float);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].stride = sizeof(Vector4);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].size   = 0;
//
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].buffer = outScene.buffer;
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].offset = outScene.rawData.tangents.size() * sizeof(float);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].stride = sizeof(Vector4);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].size   = 0;
//
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::BITANGENT)].buffer = outScene.buffer;
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::BITANGENT)].offset = outScene.rawData.biTangents.size() * sizeof(float);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::BITANGENT)].stride = sizeof(Vector4);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::BITANGENT)].size   = 0;
//
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].buffer = outScene.buffer;
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].offset = outScene.rawData.colors.size() * sizeof(float);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].stride = sizeof(Vector4);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].size   = 0;
//
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].buffer = outScene.buffer;
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].offset = outScene.rawData.uvs.size() * sizeof(float);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].stride = 2 * sizeof(float);
//        data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].size   = 0;
//
//        data.vertexDescriptions.emplace_back(VertexDesc{"inPos", 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
//        data.vertexDescriptions.emplace_back(VertexDesc{"inNormal", 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
//        data.vertexDescriptions.emplace_back(VertexDesc{"inTangent", 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
//        data.vertexDescriptions.emplace_back(VertexDesc{"inBiTangent", 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
//        data.vertexDescriptions.emplace_back(VertexDesc{"inColor", 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
//        data.vertexDescriptions.emplace_back(VertexDesc{"inUv", 5, 0, VK_FORMAT_R32G32_SFLOAT});
//
//        data.indexBuffer.buffer = outScene.buffer;
//        data.indexBuffer.offset = outScene.indices.size() * sizeof(uint32_t);
//        data.indexBuffer.stride = sizeof(uint32_t);
//        data.indexBuffer.size   = 0;

//        data.indexType = VK_INDEX_TYPE_UINT32;

//        uint32_t indexOffset  = 0;
//        uint32_t vertexOffset = 0;
//        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
//            aiMesh  *mesh      = scene->mMeshes[node->mMeshes[i]];
//            uint32_t vertexNum = mesh->mNumVertices;
//
//            ProcessSubMesh(mesh, scene, data, outScene, vertexOffset, indexOffset);
//
//            data.indexBuffer.size += mesh->mNumFaces * 3 * sizeof(uint32_t);
//            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::POSITION)].size += vertexNum * sizeof(Vector4);
//            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::NORMAL)].size += vertexNum * sizeof(Vector4);
//            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::TANGENT)].size += vertexNum * sizeof(Vector4);
//            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::BITANGENT)].size += vertexNum * sizeof(Vector4);
//            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::COLOR)].size += vertexNum * sizeof(Vector4);
//            data.vertexBuffers[static_cast<uint32_t>(MeshAttributeType::UV0)].size += vertexNum * 2 * sizeof(float);
//        }
    }

    static void ProcessNode(aiNode *node, const aiScene *scene, uint32_t parent, RenderPrefabAssetData& outScene)
    {
        auto index = static_cast<uint32_t>(outScene.nodes.size());
        outScene.nodes.emplace_back(RenderPrefabNode{});
        auto& current = outScene.nodes.back();
        current.parentIndex = parent;
        if (parent != ~(0U)) {
            outScene.nodes[parent].children.emplace_back(index);
        }

        current.localMatrix = FromAssimp(node->mTransformation);

        if (node->mNumMeshes != 0) {
            current.meshIndex = static_cast<uint32_t>(outScene.meshes.size());
        }

        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            ProcessNode(node->mChildren[i], scene, index, outScene);
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

        std::filesystem::path outPath(request.projectDir);
        std::string outFolder = outPath.append(request.name).replace_extension().string();
        std::filesystem::create_directories(outPath);
        outPath.append(request.name);

        auto asset = am->CreateAsset<RenderPrefab>(outPath.make_preferred().string());
        auto &assetData = asset->Data();

        ProcessMaterials(scene, assetData, fullPath, outFolder);
//        ProcessMeshes(scene, assetData);

//        ProcessNode(scene->mRootNode, scene, -1, assetData);



        result.products.emplace_back(BuildProduct{KEY, asset->GetUuid()});
        am->SaveAsset(asset);
    }

} // namespace sky::builder