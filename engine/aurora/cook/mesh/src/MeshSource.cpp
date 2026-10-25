#include "GltfSkinSource.h"
#include <aurora/cook/mesh/MeshSource.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <core/logger/Logger.h>
#include <core/math/MathUtil.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace sky::aurora::cook {
    namespace {
        Matrix4 FromAssimp(const aiMatrix4x4 &matrix)
        {
            Matrix4 result;
            for (uint32_t column = 0; column < 4; ++column) {
                for (uint32_t row = 0; row < 4; ++row) {
                    result[column][row] = matrix[row][column];
                }
            }
            return result;
        }
        MeshCookStatus Invalid(std::string message)
        {
            return {MeshCookError::InvalidData, std::move(message)};
        }
        bool FiniteAffine(const Matrix4 &matrix)
        {
            for (const float value : matrix.v) {
                if (!std::isfinite(value)) {
                    return false;
                }
            }
            return matrix[0][3] == 0.f && matrix[1][3] == 0.f && matrix[2][3] == 0.f && matrix[3][3] == 1.f && std::isfinite(matrix.Determinant()) &&
                   matrix.Determinant() != 0.f;
        }
        bool Near(const Matrix4 &a, const Matrix4 &b)
        {
            for (size_t i = 0; i < 16; ++i) {
                if (!std::isfinite(a.v[i]) || std::abs(a.v[i] - b.v[i]) > 1e-4f) {
                    return false;
                }
            }
            return true;
        }
        std::string Escape(const std::string &name)
        {
            std::string result;
            for (const char value : name) {
                if (value == '%') {
                    result += "%25";
                } else if (value == '/') {
                    result += "%2F";
                } else {
                    result += value;
                }
            }
            return result;
        }
        struct SceneNodes {
            std::vector<const aiNode *>                                  ordered;
            std::unordered_map<const aiNode *, std::string>              paths;
            std::unordered_map<const aiNode *, Matrix4>                  models;
            std::unordered_map<std::string, std::vector<const aiNode *>> names;
        };
        MeshCookStatus GatherNodes(const aiScene &scene, SceneNodes &out)
        {
            if (!scene.mRootNode) {
                return Invalid("model has no root node");
            }
            out.ordered.push_back(scene.mRootNode);
            for (size_t i = 0; i < out.ordered.size(); ++i) {
                const auto *node = out.ordered[i];
                if (out.paths.contains(node) || out.ordered.size() > 65536) {
                    return Invalid("invalid or oversized node hierarchy");
                }
                const std::string name = node->mName.C_Str();
                if (name.empty() || name.size() > 4096) {
                    return Invalid("node identity is empty or too long");
                }
                const auto local = FromAssimp(node->mTransformation);
                if (!FiniteAffine(local)) {
                    return Invalid("nonfinite or singular node transform: " + name);
                }
                const bool root = node == scene.mRootNode;
                if (!root && !out.paths.contains(node->mParent)) {
                    return Invalid("node parent is inconsistent");
                }
                const auto path = root ? "/" + Escape(name) : out.paths.at(node->mParent) + "/" + Escape(name);
                if (path.size() > 4096) {
                    return Invalid("node path exceeds 4096 bytes");
                }
                out.paths.emplace(node, path);
                out.models.emplace(node, root ? local : out.models.at(node->mParent) * local);
                out.names[name].push_back(node);
                std::unordered_set<std::string> children;
                for (uint32_t child = 0; child < node->mNumChildren; ++child) {
                    const auto *next = node->mChildren[child];
                    if (!next || next->mParent != node || !children.emplace(next->mName.C_Str()).second) {
                        return Invalid("ambiguous sibling paths or invalid child under " + path);
                    }
                    out.ordered.push_back(next);
                }
            }
            return {};
        }
        MeshCookStatus
        BuildBinding(const aiMesh &mesh, const aiNode &owner, const SceneNodes &nodes, MeshSourcePrimitive &primitive, CookMeshSource &out)
        {
            if (mesh.mNumBones == 0 || mesh.mNumBones > 256) {
                return Invalid("Skin requires 1..256 original slots");
            }
            const Matrix4 model = nodes.models.at(&owner);
            if (!FiniteAffine(model)) {
                return Invalid("mesh model transform is singular or nonfinite");
            }
            const Matrix4 inverseModel = model.Inverse();
            uint64_t      signature    = 14695981039346656037ULL;
            for (const float value : model.v) {
                const auto bits = std::bit_cast<uint32_t>(value == 0.f ? 0.f : value);
                for (uint32_t shift = 0; shift < 32; shift += 8) {
                    signature = (signature ^ uint8_t(bits >> shift)) * 1099511628211ULL;
                }
            }
            out.skin.input.modelSpaceSignature = signature ? signature : 1;
            std::unordered_set<const aiNode *>                    closure;
            std::unordered_map<const aiNode *, uint16_t>          firstSlots;
            std::vector<std::vector<std::pair<uint16_t, double>>> influences(mesh.mNumVertices);
            for (uint32_t slot = 0; slot < mesh.mNumBones; ++slot) {
                const auto *bone = mesh.mBones[slot];
                if (!bone) {
                    return Invalid("null mesh bone");
                }
                const auto named = nodes.names.find(bone->mName.C_Str());
                if (named != nodes.names.end() && named->second.size() > 1) {
                    return {MeshCookError::Unsupported, "Assimp bone identity is ambiguous: " + std::string(bone->mName.C_Str())};
                }
                const aiNode *target = bone->mNode;
                if (!target) {
                    const auto found = nodes.names.find(bone->mName.C_Str());
                    if (found == nodes.names.end() || found->second.size() != 1) {
                        return Invalid("missing or ambiguous bone node");
                    }
                    target = found->second.front();
                }
                if (!nodes.paths.contains(target)) {
                    return Invalid("bone node does not belong to the scene");
                }
                const Matrix4 inverseBind = FromAssimp(bone->mOffsetMatrix) * inverseModel;
                if (!FiniteAffine(inverseBind) || !Near(nodes.models.at(target) * inverseBind, Matrix4::Identity())) {
                    return Invalid("bind reference mismatch for " + nodes.paths.at(target));
                }
                out.skin.input.slots.push_back("slot/" + std::to_string(slot));
                out.skin.binds.push_back({inverseBind, nodes.paths.at(target), 0});
                for (const aiNode *node = target; node; node = node->mParent) {
                    if (!nodes.paths.contains(node)) {
                        return Invalid("bone ancestor is outside the scene");
                    }
                    closure.insert(node);
                }
                const auto [original, inserted] = firstSlots.try_emplace(target, static_cast<uint16_t>(slot));
                const auto inputSlot            = original->second;
                for (uint32_t index = 0; index < bone->mNumWeights; ++index) {
                    const auto &weight = bone->mWeights[index];
                    if (weight.mVertexId >= mesh.mNumVertices || !std::isfinite(weight.mWeight) || weight.mWeight < 0.f) {
                        return Invalid("invalid original vertex influence");
                    }
                    if (weight.mWeight == 0.f) {
                        continue;
                    }
                    auto      &values = influences[weight.mVertexId];
                    const auto found =
                        std::find_if(values.begin(), values.end(), [inputSlot](const auto &value) { return value.first == inputSlot; });
                    if (found != values.end()) {
                        found->second += weight.mWeight;
                    } else {
                        values.emplace_back(inputSlot, weight.mWeight);
                    }
                    if (values.size() > 4) {
                        return {MeshCookError::Unsupported, "vertex has more than four positive influences"};
                    }
                }
            }
            if (closure.size() > 256) {
                return {MeshCookError::Unsupported, "rig ancestor closure exceeds 256 joints"};
            }
            std::unordered_map<const aiNode *, uint16_t> rigIndices;
            for (const auto *node : nodes.ordered) {
                if (!closure.contains(node)) {
                    continue;
                }
                const Matrix4 local = FromAssimp(node->mTransformation);
                Transform     transform;
                Decompose(local, transform.translation, transform.rotation, transform.scale);
                if (!Near(transform.ToMatrix(), local)) {
                    return Invalid("reference local transform contains shear: " + nodes.paths.at(node));
                }
                const auto parent = node->mParent ? rigIndices.at(node->mParent) : uint16_t{0xFFFF};
                rigIndices.emplace(node, static_cast<uint16_t>(out.bones.size()));
                out.bones.push_back({nodes.paths.at(node), parent, transform});
            }
            primitive.joints.resize(mesh.mNumVertices);
            primitive.weights.resize(mesh.mNumVertices);
            for (uint32_t vertex = 0; vertex < mesh.mNumVertices; ++vertex) {
                const auto &values = influences[vertex];
                double      total  = 0;
                for (const auto &value : values) {
                    total += value.second;
                }
                if (!std::isfinite(total) || total <= 0) {
                    return Invalid("skinned vertex has zero or invalid total weight");
                }
                for (size_t influence = 0; influence < values.size(); ++influence) {
                    primitive.joints[vertex][influence]  = static_cast<float>(values[influence].first);
                    primitive.weights[vertex][influence] = static_cast<float>(values[influence].second / total);
                    if (primitive.weights[vertex][influence] == 0.f) {
                        return Invalid("normalized positive influence underflowed");
                    }
                }
            }
            return {};
        }
        MeshCookStatus ProcessPrimitive(const aiMesh *mesh, const aiNode &node, const SceneNodes &nodes, uint32_t materialIndex, CookMeshSource &out)
        {
            if (!mesh || !mesh->HasPositions() || !mesh->HasFaces()) {
                return Invalid("mesh primitive lacks positions or faces");
            }
            MeshSourcePrimitive prim;
            prim.transform             = nodes.models.at(&node);
            prim.materialIndex         = materialIndex;
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
                const auto status = BuildBinding(*mesh, node, nodes, prim, out);
                if (!status) {
                    return status;
                }
            }
            for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
                const auto &face = mesh->mFaces[faceIndex];
                if (face.mNumIndices != 3) {
                    return Invalid("mesh contains a non-triangle after triangulation");
                }
                for (uint32_t corner = 0; corner < 3; ++corner) {
                    if (face.mIndices[corner] >= mesh->mNumVertices) {
                        return Invalid("mesh index exceeds vertex count");
                    }
                    prim.indices.push_back(face.mIndices[corner]);
                }
            }
            out.primitives.push_back(std::move(prim));
            return {};
        }
    } // namespace

    MeshCookStatus ExtractMeshSources(const aiScene &scene, std::vector<CookMeshSource> &out)
    {
        SceneNodes nodes;
        if (const auto status = GatherNodes(scene, nodes); !status) {
            return status;
        }
        std::vector<CookMeshSource> result;
        CookMeshSource              statics;
        statics.key = "static";
        std::unordered_set<std::string> keys;
        for (const auto *node : nodes.ordered) {
            for (uint32_t entry = 0; entry < node->mNumMeshes; ++entry) {
                if (node->mMeshes[entry] >= scene.mNumMeshes) {
                    return Invalid("node mesh index is invalid");
                }
                const auto *mesh = scene.mMeshes[node->mMeshes[entry]];
                if (!mesh) {
                    return Invalid("node references a null mesh");
                }
                CookMeshSource    current;
                auto             &source   = mesh->HasBones() ? current : statics;
                const std::string meshName = mesh->mName.C_Str();
                // Assimp has no universal persistent mesh id; a path rename creates a new child identity.
                source.key =
                    mesh->HasBones() ? nodes.paths.at(node) + "/mesh/" + Escape(meshName.empty() ? std::to_string(entry) : meshName) : "static";
                if (mesh->HasBones() && !keys.emplace(source.key).second) {
                    return Invalid("ambiguous mesh binding identity: " + source.key);
                }
                aiString materialName;
                if (mesh->mMaterialIndex < scene.mNumMaterials) {
                    scene.mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, materialName);
                }
                const std::string name  = materialName.C_Str();
                const auto        found = std::find(source.materialNames.begin(), source.materialNames.end(), name);
                const auto        slot  = static_cast<uint32_t>(found - source.materialNames.begin());
                if (found == source.materialNames.end()) {
                    source.materialNames.push_back(name);
                }
                if (const auto status = ProcessPrimitive(mesh, *node, nodes, slot, source); !status) {
                    return status;
                }
                if (mesh->HasBones()) {
                    result.push_back(std::move(current));
                }
            }
        }
        if (statics.Valid()) {
            result.push_back(std::move(statics));
        }
        if (result.empty()) {
            return Invalid("model contains no mesh primitives");
        }
        std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.key < b.key; });
        for (const auto &source : result) {
            if (const auto status = ValidateMeshSource(source); !status) {
                return status;
            }
        }
        out = std::move(result);
        return {};
    }

    MeshCookStatus LoadMeshSources(const std::vector<uint8_t> &bytes, const std::string &ext, std::vector<CookMeshSource> &out)
    {
        Assimp::Importer importer;
        const uint32_t   flags =
            aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_PopulateArmatureData;
        const std::string    hint = !ext.empty() && ext[0] == '.' ? ext.substr(1) : ext;
        std::vector<uint8_t> prepared;
        if (const auto status = PrepareGltfSkinSource(bytes, hint, prepared); !status) {
            return status;
        }
        const auto *scene = importer.ReadFileFromMemory(prepared.data(), prepared.size(), flags, hint.c_str());
        if (!scene) {
            return Invalid(std::string("Assimp import failed: ") + importer.GetErrorString());
        }
        return ExtractMeshSources(*scene, out);
    }

    bool LoadMeshSource(const std::vector<uint8_t> &bytes, const std::string &ext, CookMeshSource &out)
    {
        std::vector<CookMeshSource> sources;
        const auto                  status = LoadMeshSources(bytes, ext, sources);
        if (!status || sources.size() != 1) {
            LOG_E("AuroraMeshCook", "%s", status ? "model contains multiple bindings; use LoadMeshSources" : status.message.c_str());
            return false;
        }
        out = std::move(sources.front());
        return true;
    }
} // namespace sky::aurora::cook
