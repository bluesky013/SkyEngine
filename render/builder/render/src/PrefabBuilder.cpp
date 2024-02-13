//
// Created by Zach Lee on 2023/2/20.
//

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <builder/render/ImageBuilder.h>
#include <builder/render/PrefabBuilder.h>
#include <core/logger/Logger.h>
#include <core/math/MathUtil.h>
#include <core/archive/MemoryArchive.h>

#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/RenderPrefab.h>

//#include <meshoptimizer.h>

#include <sstream>
#include <filesystem>

static const char* TAG = "PrefabBuilder";

namespace sky::builder {

    enum class MeshAttributeType : uint32_t {
        POSITION = 0,
        UV,
        NORMAL,
        TANGENT,
        COLOR,
        NUM,
    };

    struct StandardVertexData {
        Vector4 normal;
        Vector4 tangent;
        Vector4 color;
        Vector4 uv;
    };

    struct MeshBuildContext {
        std::vector<Vector4> position;
        std::vector<StandardVertexData> ext;
        std::vector<uint32_t> indices;
    };

    struct PrefabBuildContext {
        std::vector<MaterialInstanceAssetPtr> materials;
        std::vector<MeshAssetPtr> meshes;
        std::vector<BufferAssetPtr> buffers;
        std::unordered_map<std::string, ImageAssetPtr> textures;
        std::vector<RenderPrefabNode> nodes;
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

    static std::string GetIndexedName(const std::string_view &prefix, const std::string &name, const std::string_view &type, size_t index)
    {
        std::stringstream ss;
        if (!name.empty()) {
            ss << prefix << "_" << name << "_" << type;
        } else {
            ss << prefix << "_" << type << "_" << index;
        }

        return ss.str();
    }

    static Uuid ProcessTexture(const aiScene *scene, const aiString& str, PrefabBuildContext &context, const BuildRequest &request)
    {
        auto *am = AssetManager::Get();
        auto texPath = std::filesystem::path(request.fullPath).parent_path().append(str.C_Str());
        if (std::filesystem::exists(texPath)) {
            auto path = texPath.make_preferred().string();
            auto id = am->ImportAndBuildAsset(path);
            return id;
        }

        const auto *tex = scene->GetEmbeddedTexture(str.C_Str());
        if (tex == nullptr) {
            return {};
        }

        BuildRequest textureRequest = {};
        textureRequest.name = request.relativePath + std::string("\\embedded_") + str.C_Str();
        textureRequest.ext = std::string(".") + tex->achFormatHint;
        textureRequest.buildKey = ImageBuilder::KEY;
        textureRequest.rawData = tex->pcData;
        textureRequest.dataSize = tex->mWidth;
        textureRequest.name.erase(std::remove(textureRequest.name.begin(), textureRequest.name.end(), '*'), textureRequest.name.end());

        SourceAssetInfo info = {};
        auto asset = am->CreateAsset<Texture>(sky::AssetManager::GetUUIDByPath(request.name));

        BuildResult result = {};
        am->BuildAsset(textureRequest);

        return asset->GetUuid();
    }

    static MaterialInstanceAssetPtr CreateMaterialInstanceByMaterial(aiMaterial* material, PrefabBuildContext &context, const BuildRequest &request)
    {
        std::string matName = GetIndexedName(request.relativePath, material->GetName().C_Str(), "mat", context.materials.size());

        auto *am = AssetManager::Get();
        auto matInstanceId = am->GetUUIDByPath(matName);
        auto matInstance = am->CreateAsset<MaterialInstance>(matInstanceId);

        matInstance->Data().material = am->ImportAndBuildAsset("materials/StandardPBR.mat");
        return matInstance;
    }

    static void ProcessMaterials(const aiScene *scene, PrefabBuildContext &context, const BuildRequest &request)
    {
        uint32_t matSize = scene->mNumMaterials;
        for (uint32_t i = 0; i < matSize; ++i) {
            aiMaterial* material = scene->mMaterials[i];
            aiShadingMode shadingModel = aiShadingMode_Flat;
            material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

            LOG_I(TAG, "shader model %d", shadingModel);
            auto matAsset = CreateMaterialInstanceByMaterial(material, context, request);
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
            Uuid normalMap;
            Uuid emissiveMap;
            Uuid aoMap;
            Uuid baseColorMap;
            Uuid metallicRoughnessMap;

            aiString aiAlphaMode;
            if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, aiAlphaMode) == 0) {
                std::string mode = aiAlphaMode.data;
                if (mode == "MASK") {
                    useMask = true;
                }
            }

            float alphaCutoff = 0.5f;
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);

            if (material->Get(AI_MATKEY_TEXTURE_NORMALS(0), str) == 0) {
                // normalMap
                normalMap = ProcessTexture(scene, str, context, request);
                useNormalMap = static_cast<bool>(normalMap);
            }

            if (material->Get(AI_MATKEY_TEXTURE_EMISSIVE(0), str) == 0) {
                // emissiveMap
                emissiveMap = ProcessTexture(scene, str, context, request);
                useEmissiveMap = static_cast<bool>(emissiveMap);
            }

            if (material->Get(AI_MATKEY_TEXTURE_LIGHTMAP(0), str) == 0) {
                // aoMap;
                aoMap = ProcessTexture(scene, str, context, request);
                useAOMap = static_cast<bool>(aoMap);
            }

            if (shadingModel == aiShadingMode_PBR_BRDF) {
                material->Get(AI_MATKEY_BASE_COLOR, baseColor);
                material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
                material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);


                if (material->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &str) == 0) {
                    // baseColorMap
                    baseColorMap = ProcessTexture(scene, str, context, request);
                    useBaseColorMap = static_cast<bool>(baseColorMap);
                }

                if (material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &str) == 0) {
                    // metallicRoughnessMap
                    metallicRoughnessMap = ProcessTexture(scene, str, context, request);
                    useMetallicRoughnessMap = static_cast<bool>(metallicRoughnessMap);
                }
            }

            data.properties.valueMap.emplace("useAOMap", Any(useAOMap));
            if (useAOMap) {
                data.properties.valueMap.emplace("aoMap", Any(MaterialTexture{static_cast<uint32_t>(data.properties.images.size())}));
                data.properties.images.emplace_back(aoMap);
            }

            data.properties.valueMap.emplace("useEmissiveMap", Any(useEmissiveMap));
            if (useEmissiveMap) {
                data.properties.valueMap.emplace("emissiveMap", Any(MaterialTexture{static_cast<uint32_t>(data.properties.images.size())}));
                data.properties.images.emplace_back(emissiveMap);
            }

            data.properties.valueMap.emplace("useBaseColorMap", Any(useBaseColorMap));
            if (useBaseColorMap) {
                data.properties.valueMap.emplace("baseColorMap", Any(MaterialTexture{static_cast<uint32_t>(data.properties.images.size())}));
                data.properties.images.emplace_back(baseColorMap);
            }

            data.properties.valueMap.emplace("useMetallicRoughnessMap", Any(useMetallicRoughnessMap));
            if (useMetallicRoughnessMap) {
                data.properties.valueMap.emplace("metallicRoughnessMap", Any(MaterialTexture{static_cast<uint32_t>(data.properties.images.size())}));
                data.properties.images.emplace_back(metallicRoughnessMap);
            }

            data.properties.valueMap.emplace("useNormalMap", Any(useNormalMap));
            if (useNormalMap) {
                data.properties.valueMap.emplace("normalMap", Any(MaterialTexture{static_cast<uint32_t>(data.properties.images.size())}));
                data.properties.images.emplace_back(normalMap);
            }

            data.properties.valueMap.emplace("useMask",     Any(useMask));

            data.properties.valueMap.emplace("baseColor",   Any(baseColor));
            data.properties.valueMap.emplace("metallic",    Any(metallic));
            data.properties.valueMap.emplace("roughness",   Any(roughness));
            data.properties.valueMap.emplace("alphaCutoff", Any(alphaCutoff));

            context.materials.emplace_back(matAsset);
        }
    }

    static void ProcessSubMesh(aiMesh *mesh, const aiScene *scene, PrefabBuildContext& prefabContext, MeshAssetData &meshData, MeshBuildContext &context)
    {
        uint32_t vertexNum   = mesh->mNumVertices;
        SubMeshAssetData subMesh = {};
        subMesh.firstVertex = static_cast<uint32_t>(context.position.size());
        subMesh.vertexCount = mesh->mNumVertices;
        subMesh.firstIndex = static_cast<uint32_t>(context.indices.size());
        subMesh.indexCount = mesh->mNumFaces * 3;
        subMesh.material = prefabContext.materials[mesh->mMaterialIndex]->GetUuid();

        context.position.resize(context.position.size() + subMesh.vertexCount);
        context.ext.resize(context.position.size() + subMesh.vertexCount);

        subMesh.aabb.min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z};
        subMesh.aabb.max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z};

        Vector4 *position = &context.position[subMesh.firstVertex];
        StandardVertexData *vtx = &context.ext[subMesh.firstVertex];

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            auto &p = mesh->mVertices[i];
            auto &n = mesh->mNormals[i];
            auto &t = mesh->mTangents[i];
            auto &b = mesh->mBitangents[i];

            position[i].x = p.x;
            position[i].y = p.y;
            position[i].z = p.z;
            position[i].w = 1.f;
            subMesh.aabb.min = Min(subMesh.aabb.min, Vector3(p.x, p.y, p.z));
            subMesh.aabb.max = Max(subMesh.aabb.max, Vector3(p.x, p.y, p.z));

            vtx[i].normal.x = n.x;
            vtx[i].normal.y = n.y;
            vtx[i].normal.z = n.z;
            vtx[i].normal.w = 1.f;

            vtx[i].tangent.x = t.x;
            vtx[i].tangent.y = t.y;
            vtx[i].tangent.z = t.z;
            vtx[i].tangent.w = 1.f;

            if (mesh->HasVertexColors(0)) {
                auto &c  = mesh->mColors[0][i];
                vtx[i].color.x = c.r;
                vtx[i].color.y = c.g;
                vtx[i].color.z = c.b;
                vtx[i].color.w = c.a;
            } else {
                vtx[i].color.x = 1.f;
                vtx[i].color.y = 1.f;
                vtx[i].color.z = 1.f;
                vtx[i].color.w = 1.f;
            }

            if (mesh->HasTextureCoords(0)) {
                auto &u = mesh->mTextureCoords[0][i];
                vtx[i].uv.x = u.x;
                vtx[i].uv.y = u.y;
            } else {
                vtx[i].uv.x = 0.f;
                vtx[i].uv.y = 0.f;
            }
        }

        context.indices.resize(context.indices.size() + subMesh.indexCount);

        uint32_t *indices = &context.indices[subMesh.firstIndex];
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            const aiFace &face = mesh->mFaces[i];
            indices[3 * i + 0] = face.mIndices[0];
            indices[3 * i + 1] = face.mIndices[1];
            indices[3 * i + 2] = face.mIndices[2];
        }
        meshData.subMeshes.emplace_back(subMesh);
    }

    static Uuid ProcessMesh(const aiScene *scene, const aiNode *node, PrefabBuildContext& prefabContext, const BuildRequest &request)
    {
        std::string meshName = GetIndexedName(request.relativePath, node->mName.C_Str(), "mesh", prefabContext.meshes.size());

        auto *am = AssetManager::Get();
        auto meshId = am->GetUUIDByPath(meshName);
        auto mesh = am->CreateAsset<Mesh>(meshId);
        prefabContext.meshes.emplace_back(mesh);

        auto &meshData = mesh->Data();

        meshData.vertexDescriptions.emplace_back("standard");
        meshData.vertexDescriptions.emplace_back("unlit");
        meshData.vertexDescriptions.emplace_back("position_only");

        uint32_t meshNum = node->mNumMeshes;
        MeshBuildContext meshContext;
        for (uint32_t i = 0; i < meshNum; i++) {
            aiMesh *aMesh = scene->mMeshes[node->mMeshes[i]];
            ProcessSubMesh(aMesh, scene, prefabContext, meshData, meshContext);
        }

        // save vertex && index buffer
        auto bufferName =  GetIndexedName(request.relativePath, node->mName.C_Str(), "buffer", prefabContext.buffers.size());
        auto bufferId = am->GetUUIDByPath(bufferName);
        auto buffer = am->CreateAsset<Buffer>(bufferId);
        auto &bufferData = buffer->Data();
        prefabContext.buffers.emplace_back(buffer);

        MemoryArchive archive = {};
        size_t offset = 0;

        // save positions
        size_t size = meshContext.position.size() * sizeof(Vector4);
        archive.Save(reinterpret_cast<const char *>(meshContext.position.data()), size);
        meshData.vertexBuffers.emplace_back(BufferViewData{bufferId, static_cast<uint32_t>(offset), static_cast<uint32_t>(size)});
        offset += size;

        // save primitives
        size = meshContext.ext.size() * sizeof(StandardVertexData);
        archive.Save(reinterpret_cast<const char *>(meshContext.ext.data()), size);
        meshData.vertexBuffers.emplace_back(BufferViewData{bufferId, static_cast<uint32_t>(offset), static_cast<uint32_t>(size)});
        offset += size;

        // save indices
        size = meshContext.indices.size() * sizeof(uint32_t);
        archive.Save(reinterpret_cast<const char*>(meshContext.indices.data()), size);
        meshData.indexBuffer = BufferViewData{bufferId, static_cast<uint32_t>(offset), static_cast<uint32_t>(size)};

        archive.Swap(bufferData.rawData);
        return meshId;
    }

    static void ProcessNode(aiNode *node, const aiScene *scene, uint32_t parent, PrefabBuildContext& context, const BuildRequest &request)
    {
        auto *am = AssetManager::Get();
        auto index = static_cast<uint32_t>(context.nodes.size());
        context.nodes.emplace_back();
        auto& current = context.nodes.back();
        current.parentIndex = parent;
        current.localMatrix = FromAssimp(node->mTransformation);

        if (node->mNumMeshes != 0) {
            current.mesh = ProcessMesh(scene, node, context, request);
        }

        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            ProcessNode(node->mChildren[i], scene, index, context, request);
        }
    }


    void PrefabBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        Assimp::Importer importer;
        if (!std::filesystem::exists(request.fullPath)) {
            return;
        }

        const aiScene* scene = importer.ReadFile(request.fullPath.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        if((scene == nullptr) || ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0u) || (scene->mRootNode == nullptr)) {
            return;
        }
        auto *am = AssetManager::Get();

        PrefabBuildContext context;
        ProcessMaterials(scene, context, request);
        ProcessNode(scene->mRootNode, scene, -1, context, request);

        for (auto &mat : context.materials) {
            result.products.emplace_back(BuildProduct{"GFX_MATERIAL", mat});
        }

        for (auto &buffer : context.buffers) {
            result.products.emplace_back(BuildProduct{"GFX_BUFFER", buffer});
        }

        for (auto &mesh : context.meshes) {
            result.products.emplace_back(BuildProduct{"GFX_MESH", mesh});
        }

        for (auto &[key, tex] : context.textures) {
            result.products.emplace_back(BuildProduct{"GFX_TEXTURE", tex});
        }

        auto asset = am->CreateAsset<RenderPrefab>(request.uuid);
        auto &assetData = asset->Data();
        assetData.nodes = context.nodes;

        result.products.emplace_back(BuildProduct{KEY.data(), asset});
        result.success = true;
    }

} // namespace sky::builder
