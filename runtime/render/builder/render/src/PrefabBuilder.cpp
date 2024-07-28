//
// Created by Zach Lee on 2023/2/20.
//
#include <builder/render/PrefabBuilder.h>
#include <builder/render/ImageBuilder.h>

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <core/logger/Logger.h>
#include <core/math/MathUtil.h>
#include <core/archive/MemoryArchive.h>

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>

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
        std::string name;
        AssetSourcePath path;
        std::unordered_map<AssetSourcePath, AssetSourcePtr> textures;
        std::vector<AssetSourcePtr> meshes;
        std::vector<AssetSourcePtr> materials;

        std::vector<RenderPrefabNode> nodes;
    };

    static inline Vector4 FromAssimp(const aiColor4D &color)
    {
        return {color.r, color.g, color.b, color.a};
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

    static inline Vector3 FromAssimp(const aiVector3D &vec)
    {
        return {vec.x, vec.y, vec.z};
    }

    static inline Quaternion FromAssimp(const aiQuaternion &vec)
    {
        return {vec.w, vec.x, vec.y, vec.z};
    }

    static std::string GetIndexedName(const std::string &prefix, const std::string_view &type, const std::string_view &ext, size_t index)
    {
        std::stringstream ss;
        ss << prefix << "_" << type << "_" << index << "." << ext;
        return ss.str();
    }

    static void SaveEmbeddedTexture(const aiTexture* tex, const AssetSourcePath &sourcePath)
    {
        auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
        file->WriteAsArchive()->SaveRaw(reinterpret_cast<const char*>(tex->pcData), tex->mWidth);
    }

    static Uuid ProcessTexture(const aiScene *scene, const aiString& str, PrefabBuildContext &context, const AssetImportRequest &request)
    {
        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;

        auto texPath = request.filePath.Parent() / FilePath(str.C_Str());
        if (texPath.Exist()) {
            sourcePath.path = context.path.path / FilePath(str.C_Str());
            AssetDataBase::Get()->GetWorkSpaceFs()->Copy(texPath, sourcePath.path);
        } else {
            auto [tex, index] = scene->GetEmbeddedTextureAndIndex(str.C_Str());
            if (tex == nullptr) {
                return {};
            }
            sourcePath.path = context.path.path / FilePath(GetIndexedName(context.name, "texture", tex->achFormatHint, index));
            SaveEmbeddedTexture(tex, sourcePath);
        }
        auto res = context.textures.emplace(sourcePath, AssetDataBase::Get()->RegisterAsset(sourcePath));
        return res.first->second->uuid;
    }

    static void ProcessPbrBRDF(const aiScene *scene, aiMaterial *material, PrefabBuildContext &context, const AssetImportRequest &request)
    {
        std::string matName = material->GetName().length == 0 ?
                               GetIndexedName(context.name, "material", "mati", context.meshes.size()) :
                               std::string(material->GetName().C_Str()) + ".mati";

        MaterialInstanceData data = {};
        data.material = AssetDataBase::Get()->RegisterAsset("materials/standard_pbr.mat")->uuid;

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

        aiColor4D color = {};
        aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color);
        baseColor = FromAssimp(color);

        aiGetMaterialFloat(material, AI_MATKEY_METALLIC_FACTOR, &metallic);
        aiGetMaterialFloat(material, AI_MATKEY_ROUGHNESS_FACTOR, &roughness);

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

        data.properties.valueMap.emplace("useAOMap", static_cast<uint32_t>(useAOMap));
        if (useAOMap) {
            data.properties.valueMap.emplace("AoMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(aoMap);
        }

        data.properties.valueMap.emplace("useEmissiveMap", static_cast<uint32_t>(useEmissiveMap));
        if (useEmissiveMap) {
            data.properties.valueMap.emplace("EmissiveMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(emissiveMap);
        }

        data.properties.valueMap.emplace("useBaseColorMap", static_cast<uint32_t>(useBaseColorMap));
        if (useBaseColorMap) {
            data.properties.valueMap.emplace("AlbedoMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(baseColorMap);
        }

        data.properties.valueMap.emplace("useMetallicRoughnessMap", static_cast<uint32_t>(useMetallicRoughnessMap));
        if (useMetallicRoughnessMap) {
            data.properties.valueMap.emplace("MetallicRoughnessMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(metallicRoughnessMap);
        }

        data.properties.valueMap.emplace("useNormalMap", static_cast<uint32_t>(useNormalMap));
        if (useNormalMap) {
            data.properties.valueMap.emplace("NormalMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(normalMap);
        }

        data.properties.valueMap.emplace("useMask", static_cast<uint32_t>(useMask));

        data.properties.valueMap.emplace("Albedo", baseColor);
        data.properties.valueMap.emplace("Metallic", metallic);
        data.properties.valueMap.emplace("Roughness", roughness);
        data.properties.valueMap.emplace("AlphaCutoff", alphaCutoff);

        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = context.path.path / FilePath(matName);

        {
            auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
            auto archive = file->WriteAsArchive();
            JsonOutputArchive bin(*archive);
            data.SaveJson(bin);
        }

        auto source = AssetDataBase::Get()->RegisterAsset(sourcePath);
        context.materials.emplace_back(source);
    }

    static void ProcessMaterials(const aiScene *scene, PrefabBuildContext &context, const AssetImportRequest &request)
    {
        uint32_t matSize = scene->mNumMaterials;
        for (uint32_t i = 0; i < matSize; ++i) {
            aiMaterial* material = scene->mMaterials[i];
            aiShadingMode shadingModel = aiShadingMode_Flat;
            material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

            if (shadingModel == aiShadingMode_PBR_BRDF) {
                ProcessPbrBRDF(scene, material, context, request);
            } else {
                SKY_ASSERT(false);
            }
        }
    }

    static void ProcessSubMesh(aiMesh *mesh, const aiScene *scene, PrefabBuildContext& prefabContext, MeshAssetData &meshData, MeshBuildContext &context)
    {
        SubMeshAssetData subMesh = {};
        subMesh.firstVertex = static_cast<uint32_t>(context.position.size());
        subMesh.vertexCount = mesh->mNumVertices;
        subMesh.firstIndex = static_cast<uint32_t>(context.indices.size());
        subMesh.indexCount = mesh->mNumFaces * 3;
        subMesh.material = prefabContext.materials[mesh->mMaterialIndex]->uuid;

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

    static Uuid ProcessMesh(const aiScene *scene, const aiNode *node, PrefabBuildContext& context, const AssetImportRequest &request)
    {
        std::string meshName = node->mName.length == 0 ?
                GetIndexedName(context.name, "mesh", "mesh", context.meshes.size()) :
                std::string(node->mName.C_Str()) + ".mesh";

        MeshAssetData meshData;

        meshData.vertexDescriptions.emplace_back("standard");
        meshData.vertexDescriptions.emplace_back("unlit");
        meshData.vertexDescriptions.emplace_back("position_only");

        uint32_t meshNum = node->mNumMeshes;
        MeshBuildContext meshContext;
        for (uint32_t i = 0; i < meshNum; i++) {
            aiMesh *aMesh = scene->mMeshes[node->mMeshes[i]];
            ProcessSubMesh(aMesh, scene, context, meshData, meshContext);
        }
        size_t vtxCount = meshContext.position.size();
        size_t idxCount = meshContext.indices.size();

        auto posSize = static_cast<uint32_t>(vtxCount * sizeof(Vector4));
        auto stdSize = static_cast<uint32_t>(vtxCount * sizeof(StandardVertexData));

        meshData.primitives = {
                {0, posSize, sizeof(Vector4)},
                {posSize,  stdSize, sizeof(StandardVertexData)},
        };
        uint32_t idxOffset = posSize + stdSize;
        auto idxSize = static_cast<uint32_t>(meshContext.indices.size() * sizeof(uint32_t));
        meshData.indices = {idxOffset, idxSize, rhi::IndexType::U32 };
        meshData.dataSize = idxOffset + idxSize;
        meshData.rawData.storage.resize(meshData.dataSize);

        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = context.path.path / FilePath(meshName);

        memcpy(meshData.rawData.storage.data(), reinterpret_cast<const char*>(meshContext.position.data()), posSize);
        memcpy(meshData.rawData.storage.data() + posSize, reinterpret_cast<const char*>(meshContext.ext.data()), stdSize);
        memcpy(meshData.rawData.storage.data() + idxOffset, reinterpret_cast<const char*>(meshContext.indices.data()), idxSize);

        {
            auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
            auto archive = file->WriteAsArchive();
            BinaryOutputArchive bin(*archive);
            meshData.Save(bin);
        }

        auto source = AssetDataBase::Get()->RegisterAsset(sourcePath);
        context.meshes.emplace_back(source);
        return source->uuid;
    }

    static void ProcessNode(aiNode *node, const aiScene *scene, uint32_t parent, PrefabBuildContext& context, const AssetImportRequest &request) // NOLINT
    {
        auto index = static_cast<uint32_t>(context.nodes.size());
        context.nodes.emplace_back();
        auto& current = context.nodes.back();
        current.parentIndex = parent;

        aiVector3D translation;
        aiQuaternion rotation;
        aiVector3D scale;
        node->mTransformation.Decompose(scale, rotation, translation);
        current.localTransform.translation = FromAssimp(translation);
        current.localTransform.scale = FromAssimp(scale);
        current.localTransform.rotation = FromAssimp(rotation);

        if (node->mNumMeshes != 0) {
            current.mesh = ProcessMesh(scene, node, context, request);
        }

        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            ProcessNode(node->mChildren[i], scene, index, context, request);
        }
    }

    void PrefabBuilder::Import(const AssetImportRequest &request) const
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(request.filePath.GetStr(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        if((scene == nullptr) || ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0u) || (scene->mRootNode == nullptr)) {
            return;
        }

        PrefabBuildContext context;
        auto prefabName = request.filePath.FileName();
        context.path.bundle = SourceAssetBundle::WORKSPACE;
        context.path.path = FilePath("Prefabs") / prefabName;
        context.name = request.filePath.FileNameWithoutExt();

        AssetDataBase::Get()->GetWorkSpaceFs()->CreateSubSystem(context.path.path.GetStr(), true);
        ProcessMaterials(scene, context, request);
        ProcessNode(scene->mRootNode, scene, -1, context, request);

        RenderPrefabAssetData data;
        data.nodes.swap(context.nodes);

        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = context.path.path / FilePath(context.name + ".prefab");

        {
            auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
            auto archive = file->WriteAsArchive();
            JsonOutputArchive bin(*archive);
            data.SaveJson(bin);
        }

        AssetDataBase::Get()->RegisterAsset(sourcePath);
    }

    void PrefabBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
    }

} // namespace sky::builder
