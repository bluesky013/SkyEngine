//
// Created by Zach Lee on 2023/2/20.
//
#include <builder/render/PrefabBuilder.h>

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <core/logger/Logger.h>
#include <core/math/MathUtil.h>

#include <framework/asset/AssetDataBase.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/AnimationAsset.h>

#include <render/adaptor/assets/SkeletonAsset.h>
#include <render/skeleton/SkeletonMeshRenderer.h>
#include <animation/skeleton/Skeleton.h>

//#include <meshoptimizer.h>

#include <sstream>
#include <filesystem>

static const char* TAG = "PrefabBuilder";

namespace sky::builder {

#define OFFSET_OF(s, m) static_cast<uint32_t>(offsetof(s, m))

    enum class MeshAttributeType : uint32_t {
        POSITION = 0,
        UV,
        NORMAL,
        TANGENT,
        COLOR,
        NUM,
    };

    struct StandardVertexData {
        Vector4 uv;
        Vector4 normal;
        Vector4 tangent;
        Vector4 color;
    };

    struct MeshBuildContext {
        std::vector<Vector4> position;
        std::vector<StandardVertexData> ext;
        std::vector<VertexBoneData> bone;
        std::vector<uint32_t> indices;
    };

    struct PrefabBuildContext {
        std::string name;
        AssetSourcePath path;
        std::unordered_map<AssetSourcePath, AssetSourcePtr> textures;
        std::vector<AssetSourcePtr> meshes;
        std::vector<AssetSourcePtr> materials;

        SkeletonAssetData skeleton;
        AssetSourcePtr skeletonSource;
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
                               GetIndexedName(context.name, "material", "mati", context.materials.size()) :
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
        float metallic = 0.1f;
        float roughness = 1.0f;
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
        if (color.IsBlack()) {
            aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color);
        }
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

        data.properties.options.emplace("ENABLE_AO_MAP", useAOMap);
        if (useAOMap) {
            data.properties.valueMap.emplace("AoMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(aoMap);
        }

        data.properties.options.emplace("ENABLE_EMISSIVE_MAP", useEmissiveMap);
        if (useEmissiveMap) {
            data.properties.valueMap.emplace("EmissiveMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(emissiveMap);
        }

//        data.properties.valueMap.emplace("useBaseColorMap", static_cast<uint32_t>(useBaseColorMap));
        if (useBaseColorMap) {
            data.properties.valueMap.emplace("AlbedoMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(baseColorMap);
        }

        data.properties.options.emplace("ENABLE_MR_MAP", useMetallicRoughnessMap);
        if (useMetallicRoughnessMap) {
            data.properties.valueMap.emplace("MetallicRoughnessMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(metallicRoughnessMap);
        }

        data.properties.options.emplace("ENABLE_NORMAL_MAP", useNormalMap);
        if (useNormalMap) {
            data.properties.valueMap.emplace("NormalMap",
                                             MaterialTexture{static_cast<uint32_t>(data.properties.images.size())});
            data.properties.images.emplace_back(normalMap);
        }

        data.properties.options.emplace("ENABLE_ALPHA_MASK", useMask);

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

            ProcessPbrBRDF(scene, material, context, request);
//            if (shadingModel == aiShadingMode_PBR_BRDF) {
//            } else {
//            }
        }
    }

    static void ProcessSkeletonBone(aiMesh *mesh, const aiScene *scene, SkeletonAssetData &skeleton)
    {
        for (uint32_t i = 0; i < mesh->mNumBones; ++i) {
            auto &aBone = mesh->mBones[i];
            std::string boneName = aBone->mName.C_Str();

            uint32_t boneIndex = 0;
            if (auto iter = skeleton.nameToIndexMap.find(Name(aBone->mName.C_Str())); iter != skeleton.nameToIndexMap.end()) {
                boneIndex = iter->second;
            } else {
                boneIndex = skeleton.AdddBone(boneName, FromAssimp(aBone->mOffsetMatrix));
            }
        }
    }

    static void ProcessSkinData(aiMesh* mesh, PrefabBuildContext& prefabContext, MeshBuildContext &context)
    {
        auto baseVertex = static_cast<uint32_t>(context.bone.size());
        context.bone.resize(baseVertex + mesh->mNumVertices);

        for (uint32_t i = 0; i < mesh->mNumBones; ++i) {
            auto *bone = mesh->mBones[i];

            for (uint32_t j = 0; j < bone->mNumWeights; ++j) {
                uint32_t vertexId = baseVertex + bone->mWeights[j].mVertexId;
                auto &vBone = context.bone[vertexId];

                auto boneId = prefabContext.skeleton.FindBoneByName(bone->mName.C_Str());
                SKY_ASSERT(boneId < prefabContext.skeleton.nameToIndexMap.size());

                for (uint32_t k = 0; k < MAX_BONE_PER_VERTEX; ++k) {
                    if (vBone.weight[k] == 0.0f) {
                        vBone.boneId[k] = boneId;
                        vBone.weight[k] = bone->mWeights[j].mWeight;
                        break;
                    }
                }
            }
        }
    }

    static void ProcessSkeletonHierarchy(const aiScene *scene, const aiNode* node, uint32_t parentIndex, SkeletonAssetData &skeleton) // NOLINT
    {
        uint32_t boneIndex = skeleton.FindBoneByName(node->mName.C_Str());

        if (boneIndex != INVALID_BONE_ID) {
            skeleton.boneData[boneIndex].name = Name(node->mName.C_Str());
            skeleton.boneData[boneIndex].parentIndex = parentIndex;

            aiVector3t<ai_real> scaling = {};
            aiQuaterniont<ai_real> rotation = {};
            aiVector3t<ai_real> position = {};
            node->mTransformation.Decompose(scaling, rotation, position);
            skeleton.refPos[boneIndex].translation = FromAssimp(position);
            skeleton.refPos[boneIndex].rotation = FromAssimp(rotation);
            skeleton.refPos[boneIndex].scale = FromAssimp(scaling);
            parentIndex = boneIndex;
        }

        for (uint32_t i = 0 ; i < node->mNumChildren ; i++) {
            ProcessSkeletonHierarchy(scene, node->mChildren[i], parentIndex, skeleton);
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
        context.ext.resize(context.ext.size() + subMesh.vertexCount);

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

            vtx[i].uv = VEC4_ZERO;
            if (mesh->HasTextureCoords(0)) {
                auto &u = mesh->mTextureCoords[0][i];
                vtx[i].uv.x = u.x;
                vtx[i].uv.y = u.y;
            }
            if (mesh->HasTextureCoords(1)) {
                auto &u = mesh->mTextureCoords[1][i];
                vtx[i].uv.z = u.x;
                vtx[i].uv.w = u.y;
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

        uint32_t meshNum = node->mNumMeshes;
        MeshBuildContext meshContext;

        bool hasSkin = true;

        for (uint32_t i = 0; i < meshNum; ++i) {
            aiMesh *aMesh = scene->mMeshes[node->mMeshes[i]];
            ProcessSubMesh(aMesh, scene, context, meshData, meshContext);

            hasSkin &= aMesh->HasBones();
        }

        for (uint32_t i = 0; hasSkin && i < meshNum; ++i) {
            aiMesh *aMesh = scene->mMeshes[node->mMeshes[i]];
            ProcessSkinData(aMesh, context, meshContext);
            meshData.skeleton = context.skeletonSource->uuid;
        }

        size_t vtxCount = meshContext.position.size();
        size_t idxCount = meshContext.indices.size();

        auto posSize = static_cast<uint32_t>(vtxCount * sizeof(Vector4));
        SKY_ASSERT(vtxCount == meshContext.ext.size());
        auto stdSize = static_cast<uint32_t>(vtxCount * sizeof(StandardVertexData));
        SKY_ASSERT(meshContext.bone.empty() || vtxCount == meshContext.bone.size());
        auto skinSize = meshContext.bone.empty() ? 0 : static_cast<uint32_t>(vtxCount * sizeof(VertexBoneData));

        meshData.buffers = {
            MeshBufferView{0,       posSize, sizeof(Vector4)},
            MeshBufferView{posSize, stdSize, sizeof(StandardVertexData)},
        };

        meshData.attributes = {
            VertexAttribute{VertexSemanticFlagBit::POSITION, 0, 0, rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::UV, 1, OFFSET_OF(StandardVertexData, uv), rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::NORMAL, 1, OFFSET_OF(StandardVertexData, normal), rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::TANGENT, 1, OFFSET_OF(StandardVertexData, tangent), rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::COLOR, 1, OFFSET_OF(StandardVertexData, color), rhi::Format::F_RGBA32},
        };

        if (skinSize != 0) {
            meshData.buffers.emplace_back(MeshBufferView{posSize + stdSize, skinSize, static_cast<uint32_t>(sizeof(VertexBoneData))});

            meshData.attributes.emplace_back(VertexAttribute{VertexSemanticFlagBit::JOINT, 2, OFFSET_OF(VertexBoneData, boneId), rhi::Format::U_RGBA32});
            meshData.attributes.emplace_back(VertexAttribute{VertexSemanticFlagBit::WEIGHT, 2, OFFSET_OF(VertexBoneData, weight), rhi::Format::F_RGBA32});
        }
        uint32_t idxOffset = posSize + stdSize + skinSize;
        auto idxSize = static_cast<uint32_t>(meshContext.indices.size() * sizeof(uint32_t));
        meshData.indexBuffer = static_cast<uint32_t>(meshData.buffers.size());
        meshData.indexType   = rhi::IndexType::U32;

        meshData.buffers.emplace_back(MeshBufferView{idxOffset, idxSize, static_cast<uint32_t>(sizeof(uint32_t))});

        meshData.dataSize = idxOffset + idxSize;
        meshData.rawData.storage.resize(meshData.dataSize);

        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = context.path.path / FilePath(meshName);

        memcpy(meshData.rawData.storage.data(), reinterpret_cast<const char*>(meshContext.position.data()), posSize);
        memcpy(meshData.rawData.storage.data() + posSize, reinterpret_cast<const char*>(meshContext.ext.data()), stdSize);
        if (skinSize != 0) {
            memcpy(meshData.rawData.storage.data() + posSize + stdSize, reinterpret_cast<const char*>(meshContext.bone.data()), skinSize);
        }
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
        current.name = node->mName.C_Str();

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

    static void ProcessNodeChannel(const aiScene* scene, aiNodeAnim *anim, AnimNodeChannelData& channel)
    {
        channel.name = anim->mNodeName.C_Str();

        channel.position.time.resize(anim->mNumPositionKeys);
        channel.position.keys.resize(anim->mNumPositionKeys);
        for (uint32_t i = 0; i < anim->mNumPositionKeys; ++i) {
            const auto &src = anim->mPositionKeys[i];
            auto &dstTime = channel.position.time[i];
            auto &dstPos = channel.position.keys[i];
            dstTime = static_cast<float>(src.mTime);
            dstPos.x = src.mValue.x;
            dstPos.y = src.mValue.y;
            dstPos.z = src.mValue.z;
        }

        channel.scale.time.resize(anim->mNumScalingKeys);
        channel.scale.keys.resize(anim->mNumScalingKeys);
        for (uint32_t i = 0; i < anim->mNumScalingKeys; ++i) {
            const auto &src = anim->mScalingKeys[i];
            auto &dstTime = channel.scale.time[i];
            auto &dstScale = channel.scale.keys[i];
            dstTime = static_cast<float>(src.mTime);
            dstScale.x = src.mValue.x;
            dstScale.y = src.mValue.y;
            dstScale.z = src.mValue.z;
        }

        channel.rotation.time.resize(anim->mNumRotationKeys);
        channel.rotation.keys.resize(anim->mNumRotationKeys);
        for (uint32_t i = 0; i < anim->mNumRotationKeys; ++i) {
            const auto &src = anim->mRotationKeys[i];
            auto &dstTime = channel.rotation.time[i];
            auto &dstRot = channel.rotation.keys[i];
            dstTime = static_cast<float>(src.mTime);
            dstRot.x = src.mValue.x;
            dstRot.y = src.mValue.y;
            dstRot.z = src.mValue.z;
            dstRot.w = src.mValue.w;
        }
    }

    static void ProcessMeshChannel(const aiScene* scene, aiMeshAnim *anim, PrefabBuildContext& context)
    {
        // not implement yet
    }

    static void ProcessMorphMeshChannel(const aiScene* scene, aiMeshMorphAnim *anim, PrefabBuildContext& context)
    {
        // not implement yet
    }

    static void ProcessSkeleton(const aiScene* scene, PrefabBuildContext& context)
    {
        for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
            ProcessSkeletonBone(scene->mMeshes[i], scene, context.skeleton);
        }

        ProcessSkeletonHierarchy(scene, scene->mRootNode, INVALID_BONE_ID, context.skeleton);
        if (!context.skeleton.nameToIndexMap.empty()) {
            AssetSourcePath sourcePath = {};
            sourcePath.bundle = SourceAssetBundle::WORKSPACE;
            sourcePath.path = context.path.path / FilePath(context.name + ".skeleton");

            {
                auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
                auto archive = file->WriteAsArchive();
                BinaryOutputArchive bin(*archive);
                context.skeleton.Save(bin);
            }

            context.skeletonSource = AssetDataBase::Get()->RegisterAsset(sourcePath);
        }
    }

    static void ProcessAnimation(const aiScene* scene, PrefabBuildContext& context)
    {
        for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
            auto &anim = scene->mAnimations[i];

            AnimationAssetData data;
            data.version = 1;
            data.name = anim->mName.C_Str();
            data.nodeChannels.resize(anim->mNumChannels);
            // node animation
            for (uint32_t j = 0; j < anim->mNumChannels; ++j) {;
                ProcessNodeChannel(scene, anim->mChannels[j], data.nodeChannels[j]);
            }

            // mesh animation
            for (uint32_t j = 0; j < anim->mNumMeshChannels; ++j) {
                ProcessMeshChannel(scene, anim->mMeshChannels[j], context);
            }

            // morph animation
            for (uint32_t j = 0; j < anim->mNumMorphMeshChannels; ++j) {
                ProcessMorphMeshChannel(scene, anim->mMorphMeshChannels[j], context);
            }

            AssetSourcePath sourcePath = {};
            sourcePath.bundle = SourceAssetBundle::WORKSPACE;
            sourcePath.path = context.path.path / FilePath(data.name + ".anim");

            {
                auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
                auto archive = file->WriteAsArchive();
                BinaryOutputArchive bin(*archive);
                data.Save(bin);
            }

            AssetDataBase::Get()->RegisterAsset(sourcePath);
        }
    }

    void PrefabBuilder::Import(const AssetImportRequest &request) const
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(request.filePath.GetStr(),
            aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_PopulateArmatureData);
        if(scene == nullptr) {
            return;
        }

        PrefabBuildContext context;
        if ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0u) {
            ProcessAnimation(scene, context);
            return;
        }

        auto prefabName = request.filePath.FileName();
        context.path.bundle = SourceAssetBundle::WORKSPACE;
        context.path.path = FilePath("Prefabs") / prefabName;
        context.name = request.filePath.FileNameWithoutExt();

        AssetDataBase::Get()->GetWorkSpaceFs()->CreateSubSystem(context.path.path.GetStr(), true);
        ProcessMaterials(scene, context, request);

        ProcessSkeleton(scene, context);
        ProcessAnimation(scene, context);

        if (scene->mRootNode != nullptr) {
            ProcessNode(scene->mRootNode, scene, -1, context, request);
        }

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
