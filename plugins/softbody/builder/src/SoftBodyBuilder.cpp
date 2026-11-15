#include <softbody/builder/SoftBodyBuilder.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <softbody/assets/SoftBodyAsset.h>
#include <softbody/assets/SoftBodyRecipe.h>

#include <algorithm>
#include <map>

namespace sky::softbody {
    std::string_view SoftBodyBuilder::QueryType(const std::string &extension) const
    {
        return extension == ".softbodyrig" ? SOFT_BODY_ASSET_TYPE : std::string_view{};
    }

    void SoftBodyBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        result.error.clear();
        if (!request.file || !request.assetInfo) {
            result.error = "Softbody recipe source or identity is unavailable";
            return;
        }
        const auto archive = request.file->ReadAsArchive();
        if (!archive) {
            result.error = "Softbody recipe cannot be opened";
            return;
        }
        auto &stream = archive->GetStream();
        stream.seekg(0, std::ios::end);
        const auto length = stream.tellg();
        if (length <= 0 || length > 1024 * 1024) {
            result.error = "Softbody recipe exceeds its size limit";
            return;
        }
        stream.seekg(0, std::ios::beg);
        std::string text(static_cast<size_t>(length), '\0');
        if (!archive->LoadRaw(text.data(), text.size())) {
            result.error = "Softbody recipe read failed";
            return;
        }
        SoftBodyRecipe recipe;
        if (const auto status = ParseSoftBodyRecipe(text, recipe); !status) {
            result.error = status.message;
            return;
        }
        if (recipe.source == request.assetInfo->uuid) {
            result.error = "A softbody recipe cannot import itself";
            return;
        }
        const auto compiler = phy::MeshAuthoringCompilerRegistry::Get()->Acquire();
        if (!compiler) {
            result.error = "Mesh authoring compiler service is unavailable";
            return;
        }
        auto                *database = AssetDataBase::Get();
        const auto           original = database->FindAsset(recipe.source);
        const auto           file     = original ? database->OpenFile(original) : FilePtr{};
        std::vector<uint8_t> bytes;
        if (!file || !file->ReadBin(bytes)) {
            result.error = "Original mesh source cannot be read";
            return;
        }
        std::vector<phy::AuthoringMeshSource> sources;
        if (const auto status = compiler->Load(bytes, original->ext, sources); !status) {
            result.error = status.message;
            return;
        }
        const auto source = std::find_if(sources.begin(), sources.end(), [&](const auto &entry) { return entry.key == recipe.sourceKey; });
        if (source == sources.end()) {
            result.error = "Recipe mesh source key no longer exists";
            return;
        }
        RigGenerationInput input;
        input.source        = *source;
        input.bodyPrimitive = recipe.settings.bodyPrimitive;
        input.controls      = recipe.controls;
        input.attachments   = recipe.settings.attachments;
        input.settings      = recipe.settings.generator;
        RigGenerationOutput rig;
        if (const auto status = GenerateRig(input, rig); !status) {
            result.error = status.message;
            return;
        }
        uint32_t slotCount = 0;
        for (const auto &primitive : source->primitives) {
            if (primitive.materialIndex >= 8192) {
                result.error = "Mesh material slot exceeds the product limit";
                return;
            }
            slotCount = std::max(slotCount, primitive.materialIndex + 1);
        }
        if (const auto status = compiler->ResolveMaterials(request.target, slotCount, recipe.settings.materialSlots, rig.binding.materialSlots);
            !status) {
            result.error = status.message;
            return;
        }
        if (std::find(rig.binding.materialSlots.begin(), rig.binding.materialSlots.end(), request.assetInfo->uuid) !=
            rig.binding.materialSlots.end()) {
            result.error = "A softbody recipe cannot use itself as a material";
            return;
        }
        SoftBodyAssetData body;
        body.normalPolicy = recipe.settings.normalPolicy;
        if (const auto status = ConfigureRigPhysics(recipe.settings.material, rig.mesh, body.body); !status) {
            result.error = status.message;
            return;
        }
        const std::vector<GeneratedAssetDescriptor> descriptors{{"mesh", "AuroraMesh"}, {"skin", "AuroraSkin"}, {"skeleton", "AuroraSkeleton"}};
        const std::map<std::string, std::string>    defaults{{"mesh", "mesh"}, {"skin", "skin"}, {"skeleton", "skeleton"}, {"softbody", "$source"}};
        GeneratedAssetManifest                      manifest;
        if (const auto status = database->UpdateGeneratedProducts(request.assetInfo->uuid, descriptors, defaults, manifest); !status) {
            result.error = status.message;
            return;
        }
        phy::AuthoringProductIds ids;
        ids.sourceOwner = request.assetInfo->uuid;
        for (const auto &record : manifest.records) {
            if (!record.active) {
                continue;
            }
            if (record.key == "mesh") {
                ids.mesh = record.uuid;
            } else if (record.key == "skin") {
                ids.skin = record.uuid;
            } else if (record.key == "skeleton") {
                ids.skeleton = record.uuid;
            }
        }
        phy::AuthoringMeshProducts compiled;
        if (const auto status = compiler->Compile(*source, rig.binding, ids, compiled); !status) {
            result.error = status.message;
            return;
        }
        body.mesh           = ids.mesh;
        body.skin           = ids.skin;
        body.skeleton       = ids.skeleton;
        body.skeletonLayout = compiled.skeletonLayout;
        body.controlIds     = std::move(rig.controlIds);
        if (const auto status = ValidateSoftBodyAsset(body); !status) {
            result.error = status.message;
            return;
        }
        auto      *manager = AssetManager::Get();
        const auto product = manager->CreateBuildAsset(request.assetInfo->uuid, Name(SOFT_BODY_ASSET_TYPE.data()));
        if (!product) {
            result.error = "Softbody product handler is unavailable";
            return;
        }
        const auto asset = std::static_pointer_cast<Asset<SoftBodyAsset>>(product);
        asset->Data()    = std::move(body);
        asset->AddDependencies(ids.mesh);
        asset->AddDependencies(ids.skin);
        asset->AddDependencies(ids.skeleton);
        compiled.products.push_back(product);
        const auto &bundle = request.bundle.empty() ? request.target : request.bundle;
        if (const auto status = manager->SaveProducts(compiled.products, bundle); !status) {
            result.error = status.message + "; complete recipe recook required";
            return;
        }
        std::vector<Uuid> dependencies{recipe.source};
        for (const auto &material : compiled.effectiveDependencies)
            if (std::find(dependencies.begin(), dependencies.end(), material) == dependencies.end())
                dependencies.push_back(material);
        database->SetBuildDependencies(request.assetInfo->uuid, dependencies);
        request.assetInfo->dependencies = std::move(dependencies);
        result.retCode                  = AssetBuildRetCode::SUCCESS;
    }
} // namespace sky::softbody
