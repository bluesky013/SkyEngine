//
// CreateMaterialFromAsset / CreateMaterialInstanceFromAsset: runtime-only build
// from the material asset data onto aurora::Material / aurora::MaterialInstance.
// Shaders are carried as ShaderRef (resolved by the renderer); textures are
// resolved through AssetManager + CreateTextureFromAsset.
//

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>

#include <core/logger/Logger.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <framework/asset/AssetManager.h>

#include <cstring>

static const char *TAG = "AuroraMaterialAsset";

namespace sky::aurora {

    namespace {

        void ResolveTexture(Device *device, Material &material, const Name &name, const Uuid &uuid)
        {
            if (!uuid) {
                return;
            }
            auto *manager = AssetManager::Get();
            if (manager == nullptr) {
                return;
            }
            auto textureAsset = manager->LoadAsset<Texture>(uuid);
            if (!textureAsset) {
                LOG_W(TAG, "material texture asset not resolved: %s", std::string(uuid.ToString()).c_str());
                return;
            }
            const CounterPtr<Texture> &texture =
                textureAsset->GetOrCreateResource([device](Asset<Texture> &asset) { return CreateTextureFromAsset(device, asset); });
            material.SetTexture(name, texture);
        }

        void ApplyValueOverride(MaterialInstance &instance, const MaterialInstanceValueOverride &override)
        {
            const Name name(override.name.c_str());
            std::visit(
                [&](const auto &v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (!std::is_same_v<T, Uuid>) {
                        instance.SetValue(name, v);
                    }
                },
                override.value);
        }

    } // namespace

    CounterPtr<Material> CreateMaterialFromAsset(Device *device, const Asset<Material> &asset)
    {
        if (device == nullptr) {
            LOG_E(TAG, "CreateMaterialFromAsset: null device");
            return {};
        }
        const MaterialAssetData &data = asset.Data();
        if (data.version != MaterialAssetData::CURRENT_VERSION) {
            LOG_E(TAG, "unsupported material asset version: %u (expected %u)", data.version, MaterialAssetData::CURRENT_VERSION);
            return {};
        }

        CounterPtr<Material> material = new Material();

        for (const auto &src : data.techniques) {
            MaterialTechnique technique;
            technique.techniqueTag = Name(src.tag.c_str());
            technique.program      = src.program;
            technique.uniforms     = src.uniforms;
            technique.renderState  = src.renderState;
            material->AddTechnique(technique);
        }

        for (const auto &src : data.properties) {
            const Name name(src.name.c_str());
            if (IsTextureProperty(src.type)) {
                material->AddTexture(name, src.type);
                if (const Uuid *texture = std::get_if<Uuid>(&src.value); texture != nullptr) {
                    ResolveTexture(device, *material, name, *texture);
                }
            } else {
                std::visit([&](const auto &v) { material->AddValue(name, src.type, &v); }, src.value);
            }
        }

        return material;
    }

    CounterPtr<MaterialInstance> CreateMaterialInstanceFromAsset(Device *device, const Asset<MaterialInstance> &asset)
    {
        if (device == nullptr) {
            LOG_E(TAG, "CreateMaterialInstanceFromAsset: null device");
            return {};
        }
        const MaterialInstanceAssetData &data = asset.Data();
        if (data.version != MaterialInstanceAssetData::CURRENT_VERSION) {
            LOG_E(TAG, "unsupported material instance asset version: %u (expected %u)", data.version, MaterialInstanceAssetData::CURRENT_VERSION);
            return {};
        }

        auto *manager = AssetManager::Get();
        if (manager == nullptr || !data.material) {
            LOG_E(TAG, "CreateMaterialInstanceFromAsset: missing asset manager or parent material");
            return {};
        }

        auto materialAsset = manager->LoadAsset<Material>(data.material);
        if (!materialAsset) {
            LOG_W(TAG, "parent material asset not resolved: %s", std::string(data.material.ToString()).c_str());
            return {};
        }

        const CounterPtr<Material> &material =
            materialAsset->GetOrCreateResource([device](Asset<Material> &value) { return CreateMaterialFromAsset(device, value); });

        CounterPtr<MaterialInstance> instance = new MaterialInstance();
        instance->SetMaterial(material);

        for (const auto &override : data.valueOverrides) {
            ApplyValueOverride(*instance, override);
        }
        for (const auto &override : data.textureOverrides) {
            if (!override.texture) {
                continue;
            }
            auto textureAsset = manager->LoadAsset<Texture>(override.texture);
            if (!textureAsset) {
                continue;
            }
            const CounterPtr<Texture> &texture =
                textureAsset->GetOrCreateResource([device](Asset<Texture> &value) { return CreateTextureFromAsset(device, value); });
            instance->SetTexture(Name(override.name.c_str()), texture);
        }

        return instance;
    }

} // namespace sky::aurora
