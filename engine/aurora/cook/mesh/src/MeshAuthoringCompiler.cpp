#include <aurora/cook/mesh/MeshAuthoringCompiler.h>

#include <animation/core/Skeleton.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshSource.h>
#include <framework/asset/AssetManager.h>
#include <physics/softbody/IMeshAuthoringCompiler.h>

#include <algorithm>
#include <unordered_set>

namespace sky::aurora::cook {
    static_assert(phy::AUTHORING_MAX_BONES == ANIM_MAX_BONE);
    namespace {
        phy::PhysicsStatus Invalid(std::string message)
        {
            return {phy::PhysicsError::InvalidArgument, std::move(message)};
        }

        class MeshAuthoringCompiler final : public phy::IMeshAuthoringCompiler {
        public:
            phy::PhysicsStatus
            Load(std::span<const uint8_t> bytes, const std::string &extension, std::vector<phy::AuthoringMeshSource> &out) const override
            {
                std::vector<CookMeshSource> imported;
                if (const auto status = LoadMeshSources({bytes.begin(), bytes.end()}, extension, imported); !status) {
                    return Invalid(status.message);
                }
                std::vector<phy::AuthoringMeshSource> candidate;
                for (auto &mesh : imported) {
                    if (mesh.Skinned()) {
                        return {phy::PhysicsError::Unsupported, "Softbody authoring requires an unbound mesh source"};
                    }
                    phy::AuthoringMeshSource source;
                    source.key           = mesh.key;
                    source.materialNames = std::move(mesh.materialNames);
                    for (size_t index = 0; index < mesh.primitives.size(); ++index) {
                        auto                       &primitive = mesh.primitives[index];
                        phy::AuthoringMeshPrimitive converted;
                        converted.key           = source.key + "/primitive/" + std::to_string(index);
                        converted.positions     = std::move(primitive.positions);
                        converted.normals       = std::move(primitive.normals);
                        converted.tangents      = std::move(primitive.tangents);
                        converted.uvs           = std::move(primitive.uvs);
                        converted.colors        = std::move(primitive.colors);
                        converted.indices       = std::move(primitive.indices);
                        converted.materialIndex = primitive.materialIndex;
                        converted.transform     = primitive.transform;
                        source.primitives.push_back(std::move(converted));
                    }
                    candidate.push_back(std::move(source));
                }
                out = std::move(candidate);
                return {};
            }

            phy::PhysicsStatus Compile(const phy::AuthoringMeshSource  &source,
                                       const phy::AuthoringMeshBinding &binding,
                                       const phy::AuthoringProductIds  &ids,
                                       phy::AuthoringMeshProducts      &out) const override
            {
                if (!ids.sourceOwner || !ids.mesh || !ids.skin || !ids.skeleton || ids.mesh == ids.skin || ids.mesh == ids.skeleton ||
                    ids.skin == ids.skeleton || ids.sourceOwner == ids.mesh || ids.sourceOwner == ids.skin || ids.sourceOwner == ids.skeleton ||
                    !binding.layoutSignature || binding.bones.empty() || binding.bones.size() > phy::AUTHORING_MAX_BONES ||
                    source.primitives.empty() || source.primitives.size() != binding.primitives.size()) {
                    return Invalid("Invalid mesh product identities or binding layout");
                }
                for (const auto &material : binding.materialSlots) {
                    if (!material || material == ids.sourceOwner || material == ids.mesh || material == ids.skin || material == ids.skeleton) {
                        return Invalid("Material slots require nonzero material identities distinct from the authoring products");
                    }
                }
                if (!binding.materialSlots.empty()) {
                    for (const auto &primitive : source.primitives) {
                        if (primitive.materialIndex >= binding.materialSlots.size()) {
                            return Invalid("Primitive material index exceeds the supplied material slot table");
                        }
                    }
                }
                SkeletonData skeletonData;
                for (size_t i = 0; i < binding.bones.size(); ++i) {
                    const auto &bone = binding.bones[i];
                    if (bone.parent != UINT16_MAX && bone.parent >= i) {
                        return Invalid("Authoring bones must precede their children");
                    }
                    skeletonData.boneData.push_back({Name(bone.name.c_str()), bone.parent});
                    skeletonData.refPos.push_back(bone.reference);
                }
                AnimationStatus skeletonStatus;
                const auto      skeleton = Skeleton::BuildSkeleton(skeletonData, &skeletonStatus);
                if (!skeleton) {
                    return Invalid(skeletonStatus.message);
                }
                CookMeshSource assembled;
                assembled.key                            = source.key;
                assembled.materialNames                  = source.materialNames;
                assembled.skin.mode                      = SkinBindingMode::Indexed;
                assembled.skin.input.modelSpaceSignature = binding.layoutSignature;
                assembled.skin.targetLayoutSignature     = skeleton->GetLayoutSignature();
                std::vector<Matrix4> models;
                for (size_t i = 0; i < binding.bones.size(); ++i) {
                    const auto &bone  = binding.bones[i];
                    const auto  local = bone.reference.ToMatrix();
                    const auto  model = bone.parent == UINT16_MAX ? local : models[bone.parent] * local;
                    models.push_back(model);
                    assembled.bones.push_back({bone.name, bone.parent, bone.reference});
                    assembled.skin.input.slots.push_back("slot/" + std::to_string(i));
                    assembled.skin.binds.push_back({model.Inverse(), {}, static_cast<uint16_t>(i)});
                }
                phy::AuthoringMeshProducts candidate;
                candidate.skeletonLayout = skeleton->GetLayoutSignature();
                std::unordered_set<std::string> primitiveKeys;
                for (size_t index = 0; index < source.primitives.size(); ++index) {
                    const auto &primitive = source.primitives[index];
                    const auto &skin      = binding.primitives[index];
                    if (primitive.key.empty() || !primitiveKeys.insert(primitive.key).second || primitive.key != skin.key ||
                        primitive.positions.size() != skin.vertices.size() || primitive.positions.size() > UINT32_MAX) {
                        return Invalid("Primitive identity or skin vertex count mismatch");
                    }
                    MeshSourcePrimitive converted;
                    converted.positions     = primitive.positions;
                    converted.normals       = primitive.normals;
                    converted.tangents      = primitive.tangents;
                    converted.uvs           = primitive.uvs;
                    converted.colors        = primitive.colors;
                    converted.indices       = primitive.indices;
                    converted.materialIndex = primitive.materialIndex;
                    converted.transform     = primitive.transform;
                    for (size_t vertex = 0; vertex < skin.vertices.size(); ++vertex) {
                        const auto &entry = skin.vertices[vertex];
                        Vector4     joints;
                        Vector4     weights;
                        for (size_t influence = 0; influence < 4; ++influence) {
                            if (entry.bones[influence] >= binding.bones.size()) {
                                return Invalid("Authoring weight references an unknown bone");
                            }
                            // The authoring tuple retains tet identity; the renderer requires empty slots to name root.
                            joints[influence]  = entry.weights[influence] == 0.f ? 0.f : static_cast<float>(entry.bones[influence]);
                            weights[influence] = entry.weights[influence];
                        }
                        converted.joints.push_back(joints);
                        converted.weights.push_back(weights);
                        candidate.vertexSources.push_back({static_cast<uint32_t>(index), static_cast<uint32_t>(vertex)});
                    }
                    assembled.primitives.push_back(std::move(converted));
                }
                MeshBuildConfig config;
                config.optimize = false;
                CookedMesh cooked;
                if (const auto status = MeshAssembler({&assembled, &config, &cooked}).DoWork(); !status) {
                    return Invalid(status.message);
                }
                auto      *manager         = AssetManager::Get();
                const auto meshProduct     = manager->CreateBuildAsset(ids.mesh, Name("AuroraMesh"));
                const auto skinProduct     = manager->CreateBuildAsset(ids.skin, Name("AuroraSkin"));
                const auto skeletonProduct = manager->CreateBuildAsset(ids.skeleton, Name("AuroraSkeleton"));
                if (!meshProduct || !skinProduct || !skeletonProduct) {
                    return {phy::PhysicsError::InvalidState, "Mesh, Skin or Skeleton asset handler is unavailable"};
                }
                const auto meshAsset = std::static_pointer_cast<Asset<Mesh>>(meshProduct);
                WriteMeshAsset(cooked, meshAsset->Data());
                meshAsset->Data().materials = binding.materialSlots;
                std::vector<Uuid> dependencies;
                for (const auto &material : binding.materialSlots) {
                    if (std::find(dependencies.begin(), dependencies.end(), material) == dependencies.end()) {
                        meshAsset->AddDependencies(material);
                        dependencies.push_back(material);
                    }
                }
                meshAsset->AddBulk(cooked.vertexData.data(), cooked.vertexData.size());
                meshAsset->AddBulk(cooked.indexData.data(), cooked.indexData.size());
                meshAsset->Data().vertexData.clear();
                meshAsset->Data().indexData.clear();
                const auto skinAsset          = std::static_pointer_cast<Asset<Skin>>(skinProduct);
                skinAsset->Data().sourceOwner = ids.sourceOwner;
                skinAsset->Data().binding     = std::move(assembled.skin);
                WriteSkeletonAsset(assembled, std::static_pointer_cast<Asset<Skeleton>>(skeletonProduct)->Data());
                candidate.products = {meshProduct, skinProduct, skeletonProduct};
                out                = std::move(candidate);
                return {};
            }
        };
    } // namespace

    std::shared_ptr<phy::IMeshAuthoringCompiler> CreateMeshAuthoringCompiler()
    {
        return std::make_shared<MeshAuthoringCompiler>();
    }
} // namespace sky::aurora::cook
