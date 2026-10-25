//
// Aurora mesh builder (see AuroraMeshBuilder.h).
//

#include <aurora/cook/mesh/AuroraMeshBuilder.h>

#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshOptimizer.h>
#include <aurora/cook/mesh/MeshSource.h>
#include <framework/asset/AssetDataBase.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

#include <cmath>
#include <core/logger/Logger.h>
#include <unordered_set>

static const char *TAG = "AuroraMeshBuilder";

namespace sky::aurora {

    std::string_view AuroraMeshBuilder::QueryType(const std::string &) const
    {
        return AssetTraits<Mesh>::ASSET_TYPE;
    }

    namespace {

        bool ReadLegacyMeshRig(BinaryInputArchive &archive, uint64_t bulkSize, Uuid &rig)
        {
            uint64_t offset = 0, size = 0;
            uint32_t stride = 0, vertices = 0, count = 0;
            archive.LoadValue(offset);
            archive.LoadValue(size);
            archive.LoadValue(stride);
            archive.LoadValue(vertices);
            archive.LoadValue(count);
            if (!archive.IsValid() || count > 16 || !vertices || vertices > 16U * 1024U * 1024U || !stride || stride > 256 ||
                size != uint64_t(vertices) * stride || offset > bulkSize || size > bulkSize - offset)
                return false;
            for (uint32_t i = 0; i < count; ++i) {
                MeshVertexAttributeData attribute;
                archive.LoadValue(attribute.semantic);
                archive.LoadValue(attribute.semanticIndex);
                archive.LoadValue(attribute.format);
                archive.LoadValue(attribute.offset);
            }
            uint32_t indices = 0, indexType = 0, bones = 0, mappings = 0;
            archive.LoadValue(offset);
            archive.LoadValue(size);
            archive.LoadValue(indices);
            archive.LoadValue(indexType);
            if (!archive.IsValid() || !indices || indices % 3 || (indexType != uint32_t(IndexType::U16) && indexType != uint32_t(IndexType::U32)) ||
                size != uint64_t(indices) * (indexType == uint32_t(IndexType::U16) ? 2 : 4) || offset > bulkSize || size > bulkSize - offset)
                return false;
            archive.LoadValue(offset);
            archive.LoadValue(size);
            archive.LoadValue(bones);
            archive.LoadValue(mappings);
            archive.LoadValue(count);
            if (!archive.IsValid() || !bones || bones > 256 || mappings > 256 || count != bones ||
                size != uint64_t(bones) * sizeof(Matrix4) + uint64_t(mappings) * sizeof(uint32_t) || offset > bulkSize || size > bulkSize - offset)
                return false;
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t length = 0;
                archive.LoadValue(length);
                if (!archive.IsValid() || !length || length > 4096)
                    return false;
                std::string name(length, '\0');
                archive.LoadValue(name.data(), length);
                if (name.find('\0') != std::string::npos)
                    return false;
            }
            archive.LoadValue(rig);
            archive.LoadValue(count);
            if (!archive.IsValid() || !rig || !count || count > 8192)
                return false;
            for (uint32_t i = 0; i < count; ++i) {
                MeshSubMeshData sub;
                archive.LoadValue(sub.indexOffset);
                archive.LoadValue(sub.indexCount);
                archive.LoadValue(sub.materialIndex);
            }
            archive.LoadValue(count);
            if (!archive.IsValid() || count > 8192)
                return false;
            for (uint32_t i = 0; i < count; ++i) {
                Uuid material;
                archive.LoadValue(material);
            }
            BoundingBoxSphere bounds;
            archive.LoadValue(bounds.center);
            archive.LoadValue(bounds.extent);
            archive.LoadValue(bounds.radius);
            return archive.IsValid();
        }

        bool SameRig(const SkeletonAssetData &legacy, const cook::CookMeshSource &source)
        {
            if (legacy.boneData.size() != source.bones.size())
                return false;
            for (size_t i = 0; i < source.bones.size(); ++i) {
                const auto &bone = source.bones[i];
                if (std::string_view(legacy.boneData[i].name.GetStr()) != bone.name || legacy.boneData[i].parentIndex != bone.parentIndex)
                    return false;
                const auto oldMatrix = legacy.refPos[i].ToMatrix(), newMatrix = bone.refTransform.ToMatrix();
                for (size_t element = 0; element < 16; ++element)
                    if (std::abs(oldMatrix.v[element] - newMatrix.v[element]) > 1e-5f)
                        return false;
            }
            return true;
        }

        void MigrateLegacyRig(const Uuid                              &owner,
                              const ProductBundleKey                  &bundle,
                              const std::vector<cook::CookMeshSource> &sources,
                              std::vector<GeneratedAssetDescriptor>   &descriptors)
        {
            std::map<std::string, Uuid> defaults;
            auto                       *database    = AssetDataBase::Get();
            bool                        hasChildren = false;
            database->ForEachGenerated([&](const GeneratedAssetRecord &record) { hasChildren |= record.owner == owner; });
            if (hasChildren)
                return;
            if (database->GetGeneratedDefaults(owner, defaults) && !defaults.empty())
                return;
            auto                  *manager = AssetManager::Get();
            AssetProductInspection mesh;
            const auto             inspected = manager->InspectProduct(owner, bundle, mesh);
            if (!inspected) {
                if (inspected.error != ProductError::NotFound)
                    LOG_W(TAG, "Previous mesh product is unreadable; legacy Skeleton references require explicit rebind");
                return;
            }
            if (mesh.type != "AuroraMesh")
                return;
            BinaryInputArchive meshHeader(*mesh.payload);
            uint32_t           version = 0;
            meshHeader.LoadValue(version);
            if (version != 5)
                return;
            Uuid                   id;
            AssetProductInspection skeleton;
            if (!ReadLegacyMeshRig(meshHeader, mesh.bulkSize, id) ||
                std::find(mesh.dependencies.begin(), mesh.dependencies.end(), id) == mesh.dependencies.end() ||
                !manager->InspectProduct(id, bundle, skeleton) || skeleton.type != "AuroraSkeleton") {
                LOG_W(TAG, "Legacy Mesh v5 has no verifiable Skeleton product; explicitly rebind old Skeleton references after recook");
                return;
            }
            SkeletonAssetData  legacy;
            BinaryInputArchive skeletonHeader(*skeleton.payload);
            legacy.Load(skeletonHeader);
            std::string key;
            size_t      matches = 0;
            if (legacy.IsLoadValid())
                for (const auto &source : sources)
                    if (source.Skinned() && SameRig(legacy, source)) {
                        key = "skeleton:" + source.key;
                        ++matches;
                    }
            if (matches != 1) {
                LOG_W(TAG, "Legacy Skeleton has no unique exact layout/reference-pose match; explicitly rebind old references after recook");
                return;
            }
            for (auto &descriptor : descriptors)
                if (descriptor.key == key)
                    descriptor.verifiedLegacyId = id;
        }

        // Move the build-scratch vertex/index streams into the product bulk
        // (the header keeps only the section table). Order must match the offsets
        // written by WriteMeshAsset.
        void CommitMeshBulk(Asset<Mesh> &asset)
        {
            auto &data = asset.Data();
            if (!data.vertexData.empty()) {
                asset.AddBulk(data.vertexData.data(), data.vertexData.size());
            }
            if (!data.indexData.empty()) {
                asset.AddBulk(data.indexData.data(), data.indexData.size());
            }
            data.vertexData.clear();
            data.indexData.clear();
        }

    } // namespace

    void AuroraMeshBuilder::LoadConfig(const FileSystemPtr &cfg)
    {
        if (cfg == nullptr) {
            return;
        }
        auto file = cfg->OpenFile(FilePath("mesh_build_presets.json"));
        if (!file) {
            LOG_W(TAG, "mesh_build_presets.json not found; using fallback config");
            return;
        }

        auto             archive = file->ReadAsArchive();
        JsonInputArchive json(*archive);
        presets.LoadJson(json);
        LOG_I(TAG, "loaded %u mesh build bundles (default '%s')", static_cast<uint32_t>(presets.bundles.size()), presets.defaultBundle.c_str());
    }

    void AuroraMeshBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        result.error.clear();
        if (!request.assetInfo || !request.file) {
            result.error = "mesh cook request lacks source data";
            return;
        }

        std::string                  bundleKey;
        const cook::MeshBuildConfig *config = presets.Resolve(request.target, bundleKey);
        if (config == nullptr) {
            result.error = "no mesh build configuration for target '" + request.target + "'";
            LOG_E(TAG, "%s", result.error.c_str());
            return;
        }

        std::vector<uint8_t> bytes;
        if (!request.file->ReadBin(bytes) || bytes.empty()) {
            result.error = "mesh source read failed: " + request.assetInfo->path.GetStr();
            LOG_E(TAG, "%s", result.error.c_str());
            return;
        }

        std::vector<cook::CookMeshSource> sources;
        if (const auto status = cook::LoadMeshSources(bytes, request.assetInfo->ext, sources); !status) {
            result.error = status.message;
            return;
        }
        std::vector<cook::CookedMesh> cooked(sources.size());
        for (size_t i = 0; i < sources.size(); ++i) {
            if (!config->skinning) {
                sources[i].skin = {};
                sources[i].bones.clear();
                for (auto &primitive : sources[i].primitives) {
                    primitive.joints.clear();
                    primitive.weights.clear();
                }
            }
            if (const auto status = cook::MeshAssembler({&sources[i], config, &cooked[i]}).DoWork(); !status) {
                result.error = status.message;
                return;
            }
            if (config->optimize) {
                if (const auto status = cook::MeshOptimizer({&cooked[i]}).DoWork(); !status) {
                    result.error = status.message;
                    return;
                }
            }
        }
        std::vector<GeneratedAssetDescriptor> descriptors;
        std::map<std::string, std::string>    defaults;
        std::vector<std::string>              meshKeys(sources.size());
        const auto                            sameGeometry = [](const cook::CookedMesh &a, const cook::CookedMesh &b) {
            if (a.vertexStride != b.vertexStride || a.indexType != b.indexType || a.vertexData != b.vertexData || a.indexData != b.indexData ||
                a.skinInput != b.skinInput || a.attributes.size() != b.attributes.size() || a.subMeshes.size() != b.subMeshes.size()) {
                return false;
            }
            for (size_t i = 0; i < a.attributes.size(); ++i) {
                const auto &x = a.attributes[i];
                const auto &y = b.attributes[i];
                if (x.semantic != y.semantic || x.semanticIndex != y.semanticIndex || x.format != y.format || x.offset != y.offset) {
                    return false;
                }
            }
            for (size_t i = 0; i < a.subMeshes.size(); ++i) {
                const auto &x = a.subMeshes[i];
                const auto &y = b.subMeshes[i];
                if (x.indexOffset != y.indexOffset || x.indexCount != y.indexCount || x.materialIndex != y.materialIndex) {
                    return false;
                }
            }
            return true;
        };
        for (size_t i = 0; i < sources.size(); ++i) {
            meshKeys[i] = i == 0 ? "$source" : "mesh:" + sources[i].key;
            for (size_t previous = 0; previous < i; ++previous) {
                if (sources[i].materialNames == sources[previous].materialNames && sameGeometry(cooked[i], cooked[previous])) {
                    meshKeys[i] = meshKeys[previous];
                    break;
                }
            }
            if (i && meshKeys[i] == "mesh:" + sources[i].key) {
                descriptors.push_back({meshKeys[i], "AuroraMesh"});
            }
            defaults["binding:" + sources[i].key + ":mesh"] = meshKeys[i];
            if (sources[i].Skinned()) {
                const auto skinKey = "skin:" + sources[i].key;
                const auto rigKey  = "skeleton:" + sources[i].key;
                descriptors.push_back({skinKey, "AuroraSkin"});
                descriptors.push_back({rigKey, "AuroraSkeleton"});
                defaults["binding:" + sources[i].key + ":skin"]     = skinKey;
                defaults["binding:" + sources[i].key + ":skeleton"] = rigKey;
                if (i == 0) {
                    defaults["skin"]     = skinKey;
                    defaults["skeleton"] = rigKey;
                }
            }
        }
        defaults["mesh"]  = "$source";
        const auto bundle = request.bundle.empty() ? bundleKey : request.bundle;
        MigrateLegacyRig(request.assetInfo->uuid, bundle, sources, descriptors);
        GeneratedAssetManifest manifest;
        if (const auto status = AssetDataBase::Get()->UpdateGeneratedProducts(request.assetInfo->uuid, descriptors, defaults, manifest); !status) {
            result.error = status.message;
            return;
        }
        std::map<std::string, Uuid> ids{{"$source", request.assetInfo->uuid}};
        std::vector<Uuid>           members{request.assetInfo->uuid};
        for (const auto &record : manifest.records) {
            if (record.active) {
                ids.emplace(record.key, record.uuid);
                members.push_back(record.uuid);
            }
        }
        auto                   *manager = AssetManager::Get();
        ProductGroupTransaction transaction;
        if (const auto status = manager->BeginProductGroup(request.assetInfo->uuid, bundle, members, transaction); !status) {
            result.error = status.message;
            return;
        }
        std::vector<AssetPtr>    products;
        std::unordered_set<Uuid> meshes;
        for (size_t i = 0; i < sources.size(); ++i) {
            const auto meshId = ids.at(meshKeys[i]);
            if (meshes.insert(meshId).second) {
                const auto product = manager->CreateBuildAsset(meshId, Name("AuroraMesh"));
                if (!product) {
                    result.error = "AuroraMesh handler unavailable";
                    return;
                }
                const auto asset = std::static_pointer_cast<Asset<Mesh>>(product);
                cook::WriteMeshAsset(cooked[i], asset->Data());
                CommitMeshBulk(*asset);
                products.push_back(product);
            }
            if (!sources[i].Skinned()) {
                continue;
            }
            const auto skinProduct = manager->CreateBuildAsset(ids.at("skin:" + sources[i].key), Name("AuroraSkin"));
            const auto rigProduct  = manager->CreateBuildAsset(ids.at("skeleton:" + sources[i].key), Name("AuroraSkeleton"));
            if (!skinProduct || !rigProduct) {
                result.error = "Skin or Skeleton handler unavailable";
                return;
            }
            const auto skinAsset          = std::static_pointer_cast<Asset<Skin>>(skinProduct);
            skinAsset->Data().binding     = sources[i].skin;
            skinAsset->Data().sourceOwner = request.assetInfo->uuid;
            cook::WriteSkeletonAsset(sources[i], std::static_pointer_cast<Asset<sky::Skeleton>>(rigProduct)->Data());
            products.push_back(skinProduct);
            products.push_back(rigProduct);
        }
        ProductGroupSnapshotPtr published;
        if (const auto status = manager->SaveProductGroup(transaction, bundle, products, published); !status) {
            result.error = status.message;
            return;
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
    }
} // namespace sky::aurora
