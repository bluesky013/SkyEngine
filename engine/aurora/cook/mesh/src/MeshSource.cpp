//
// Mesh source import via assimp (see MeshSource.h).
//

#include <aurora/cook/mesh/MeshSource.h>

#include <core/logger/Logger.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <unordered_map>

static const char *TAG = "AuroraMeshCook";

namespace sky::aurora::cook {

    namespace {

        // assimp matrices are row-major; engine Matrix4 rows map transposed
        Matrix4 FromAssimp(const aiMatrix4x4 &m)
        {
            Matrix4 res;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    res[i][j] = m[j][i];
                }
            }
            return res;
        }

        uint32_t GlobalBoneIndex(std::unordered_map<std::string, uint32_t> &table, SkinAssetData &skin,
                                 const aiBone *bone)
        {
            const std::string name = bone->mName.C_Str();
            auto iter = table.find(name);
            if (iter != table.end()) {
                return iter->second;
            }
            const uint32_t index = static_cast<uint32_t>(skin.boneNames.size());
            skin.boneNames.emplace_back(name);
            skin.inverseBindMatrices.emplace_back(FromAssimp(bone->mOffsetMatrix));
            table.emplace(name, index);
            return index;
        }

        void ProcessPrimitive(const aiMesh *mesh, const Matrix4 &transform, uint32_t materialIndex,
                              std::unordered_map<std::string, uint32_t> &boneTable, CookMeshSource &out)
        {
            if (!mesh->HasPositions() || !mesh->HasFaces()) {
                return;
            }

            MeshSourcePrimitive prim;
            prim.transform     = transform;
            prim.materialIndex = materialIndex;

            const uint32_t vertexCount = mesh->mNumVertices;
            prim.positions.reserve(vertexCount);
            for (uint32_t i = 0; i < vertexCount; ++i) {
                const auto &v = mesh->mVertices[i];
                prim.positions.emplace_back(Vector3{v.x, v.y, v.z});
            }

            if (mesh->HasNormals()) {
                prim.normals.reserve(vertexCount);
                for (uint32_t i = 0; i < vertexCount; ++i) {
                    const auto &n = mesh->mNormals[i];
                    prim.normals.emplace_back(Vector3{n.x, n.y, n.z});
                }
            }

            if (mesh->HasTangentsAndBitangents()) {
                prim.tangents.reserve(vertexCount);
                for (uint32_t i = 0; i < vertexCount; ++i) {
                    const auto &t = mesh->mTangents[i];
                    const auto &b = mesh->mBitangents[i];
                    const auto &n = mesh->mNormals[i];
                    // bitangent sign: recover handedness for the w component
                    const aiVector3D cross = n ^ t;
                    const float      sign  = (cross * b) < 0.f ? -1.f : 1.f;
                    prim.tangents.emplace_back(Vector4{t.x, t.y, t.z, sign});
                }
            }

            if (mesh->HasTextureCoords(0)) {
                prim.uvs.reserve(vertexCount);
                for (uint32_t i = 0; i < vertexCount; ++i) {
                    const auto &uv = mesh->mTextureCoords[0][i];
                    prim.uvs.emplace_back(Vector2{uv.x, uv.y});
                }
            }

            if (mesh->HasVertexColors(0)) {
                prim.colors.reserve(vertexCount);
                for (uint32_t i = 0; i < vertexCount; ++i) {
                    const auto &c = mesh->mColors[0][i];
                    prim.colors.emplace_back(Vector4{c.r, c.g, c.b, c.a});
                }
            }

            if (mesh->HasBones()) {
                prim.joints.resize(vertexCount, Vector4{});
                prim.weights.resize(vertexCount, Vector4{});
                for (uint32_t b = 0; b < mesh->mNumBones; ++b) {
                    const aiBone *bone   = mesh->mBones[b];
                    const uint32_t index = GlobalBoneIndex(boneTable, out.skin, bone);
                    for (uint32_t w = 0; w < bone->mNumWeights; ++w) {
                        const auto &weight = bone->mWeights[w];
                        if (weight.mVertexId >= vertexCount) {
                            continue;
                        }
                        auto &joints  = prim.joints[weight.mVertexId];
                        auto &weights = prim.weights[weight.mVertexId];
                        // LimitBoneWeights keeps at most 4 influences per vertex
                        for (uint32_t slot = 0; slot < 4; ++slot) {
                            if (weights[slot] == 0.f) {
                                joints[slot]  = static_cast<float>(index);
                                weights[slot] = weight.mWeight;
                                break;
                            }
                        }
                    }
                }
            }

            prim.indices.reserve(mesh->mNumFaces * 3);
            for (uint32_t i = 0; i < mesh->mNumFaces; ++i) {
                const auto &face = mesh->mFaces[i];
                if (face.mNumIndices != 3) {
                    continue; // Triangulate flag should prevent this
                }
                prim.indices.emplace_back(face.mIndices[0]);
                prim.indices.emplace_back(face.mIndices[1]);
                prim.indices.emplace_back(face.mIndices[2]);
            }

            out.primitives.emplace_back(std::move(prim));
        }

        void ProcessNode(const aiNode *node, const Matrix4 &parent, const aiScene *scene,
                         std::unordered_map<std::string, uint32_t> &materialTable,
                         std::unordered_map<std::string, uint32_t> &boneTable, CookMeshSource &out)
        {
            const Matrix4 local     = FromAssimp(node->mTransformation);
            const Matrix4 transform = parent * local;

            for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
                const aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
                if (mesh->mMaterialIndex < scene->mNumMaterials) {
                    const aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
                    aiString          name;
                    material->Get(AI_MATKEY_NAME, name);
                    const std::string materialName = name.C_Str();
                    auto iter = materialTable.find(materialName);
                    uint32_t    slot = 0;
                    if (iter != materialTable.end()) {
                        slot = iter->second;
                    } else {
                        slot = static_cast<uint32_t>(out.materialNames.size());
                        out.materialNames.emplace_back(materialName);
                        materialTable.emplace(materialName, slot);
                    }
                    ProcessPrimitive(mesh, transform, slot, boneTable, out);
                }
            }

            for (uint32_t i = 0; i < node->mNumChildren; ++i) {
                ProcessNode(node->mChildren[i], transform, scene, materialTable, boneTable, out);
            }
        }

    } // namespace

    bool LoadMeshSource(const std::vector<uint8_t> &bytes, const std::string &ext, CookMeshSource &out)
    {
        Assimp::Importer importer;
        const uint32_t flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs |
                               aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights | aiProcess_OptimizeGraph |
                               aiProcess_OptimizeMeshes | aiProcess_PopulateArmatureData;

        const std::string hint      = !ext.empty() && ext[0] == '.' ? ext.substr(1) : ext;
        const aiScene    *scene = importer.ReadFileFromMemory(bytes.data(), bytes.size(), flags, hint.c_str());
        if (scene == nullptr || scene->mRootNode == nullptr) {
            LOG_E(TAG, "assimp import failed: %s", importer.GetErrorString());
            return false;
        }

        std::unordered_map<std::string, uint32_t> materialTable;
        std::unordered_map<std::string, uint32_t> boneTable;
        ProcessNode(scene->mRootNode, Matrix4::Identity(), scene, materialTable, boneTable, out);
        if (!out.Valid()) {
            LOG_E(TAG, "assimp import produced no valid primitives");
            return false;
        }
        return true;
    }

} // namespace sky::aurora::cook
