//
// Created by Zach Lee on 2022/1/27.
//

#include <model/ModelBuilder.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <core/logger/Logger.h>
#include <engine/BasicSerialization.h>
#include <framework/asset/AssetManager.h>

static const char* TAG = "Model Loader";

namespace sky {

    ModelBuilder::ModelBuilder()
    {
        AssetManager::Get()->RegisterHandler<MeshAsset>();
    }

    ModelBuilder::~ModelBuilder()
    {
        AssetManager::Get()->UnRegisterHandler<MeshAsset>();
    }

    bool ModelBuilder::Load(const std::string& path)
    {
        scene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
        if (scene == nullptr) {
            LOG_E(TAG, "Parse Model Failed, %s", path.c_str());
            return false;
        }

        asset = AssetManager::Get()->FindOrCreate<MeshAsset>(Uuid::Create());

        LoadMesh();
//        LoadTextures();
//        LoadMaterial();
        return true;
    }

    void ModelBuilder::LoadMesh()
    {
        asset->data.meshes.resize(scene->mNumMeshes);
        for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
            auto mesh = scene->mMeshes[i];
            auto& tmpMesh = asset->data.meshes[i];

            tmpMesh.vertices.resize(mesh->mNumVertices);
            for (uint32_t j = 0; j < mesh->mNumVertices; ++j) {
                auto& vtx = tmpMesh.vertices[j];
                auto& aiVtx = mesh->mVertices[j];
                auto& aiNml = mesh->mNormals[j];

                vtx.pos = Vector4(aiVtx.x, aiVtx.y, aiVtx.z, 1.f);
                vtx.normal = Vector4(aiNml.x, aiNml.y, aiNml.z, 1.f);
                if (mesh->mColors[0] != nullptr) {
                    auto& aiColor0 = mesh->mColors[0][j];
                    vtx.color = Vector4 (aiColor0.r, aiColor0.g, aiColor0.b, aiColor0.a);
                }
            }

            tmpMesh.indices.reserve(mesh->mNumFaces * 4);
            for (uint32_t j = 0; j < mesh->mNumFaces; ++j) {
                auto& face = mesh->mFaces[j];
                for (uint32_t k = 0; k < face.mNumIndices; ++k) {
                    tmpMesh.indices.emplace_back(face.mIndices[k]);
                }
            }

            tmpMesh.aabb.min = Vector3(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z);
            tmpMesh.aabb.max = Vector3(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z);
        }
    }

    void ModelBuilder::LoadTextures()
    {
        for (uint32_t i = 0; i < scene->mNumTextures; ++i) {
            auto tex = scene->mTextures[i];
            LOG_I(TAG, "texture -%u, -%u, -%p, -%s", tex->mWidth, tex->mHeight, tex->pcData, tex->mFilename.data);
        }
    }

    void ModelBuilder::LoadMaterial()
    {
        for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
            auto mat = scene->mMaterials[i];
            for (uint32_t j = 0; j < mat->mNumProperties; ++j) {
                auto prop = mat->mProperties[j];
                if (prop->mType == aiPTI_Double) {
                    LOG_I(TAG, "double property -%s, -%f", prop->mKey.data, *(double*)prop->mData);
                } else if (prop->mType == aiPTI_Float) {
                    LOG_I(TAG, "float property -%s, -%f", prop->mKey.data, *(float*)prop->mData);
                } else if (prop->mType == aiPTI_Integer) {
                    LOG_I(TAG, "integer property -%s, -%u", prop->mKey.data, *(int*)prop->mData);
                } else if (prop->mType == aiPTI_String) {
                    char str[256];
                    uint32_t length = static_cast<unsigned int>(*reinterpret_cast<uint32_t *>(prop->mData));
                    memcpy(str, prop->mData + 4, length + 1);
                    LOG_I(TAG, "string property -%s, -%s", prop->mKey.data, str);
                }
            }
        }
    }

    void ModelBuilder::Save(const std::string& path)
    {
        AssetManager::Get()->SaveAsset(path, asset, MeshAsset::TYPE);
    }

}