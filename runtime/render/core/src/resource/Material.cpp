//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Material.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <core/template/Overloaded.h>

namespace sky {
    MaterialPropertyInitializer::MaterialPropertyInitializer(size_t propertyNum)
    {
        storage.resize(sizeof(MaterialValue) * propertyNum);
    }

    void MaterialPropertyInitializer::AddPropertyTexture(const sky::Name &name, const sky::RDTexturePtr &tex)
    {
        MaterialPropertyEntry entry = {};
        entry.size = 0;
        entry.offset = static_cast<uint16_t>(textures.size());
        textures.emplace_back(tex);

        properties[name] = entry;
    }

    void MaterialPropertyInitializer::AddPropertyValue(const Name& name, const MaterialValue &value)
    {
        SKY_ASSERT(!properties.count(name));

        MaterialPropertyEntry entry = {};
        entry.offset = currentSize;

        uint8_t *ptr = storage.data() + currentSize;
        std::visit(Overloaded{
            [&](const MaterialTexture &v) {
                SKY_ASSERT(0 && "use AddPropertyTexture")
            },
            [&](const TextureSampler &v) {
                entry.size = sizeof(TextureSampler);
                new (ptr) TextureSampler(v);
            },
            [&](const Vector2 &v) {
                entry.size = sizeof(Vector2);
                new (ptr) Vector2(v);
            },
            [&](const Vector3 &v) {
                entry.size = sizeof(Vector3);
                new (ptr) Vector3(v);
            },
            [&](const Vector4 &v) {
                entry.size = sizeof(Vector4);
                new (ptr) Vector4(v);
            },
            [&](const float &v) {
                entry.size = sizeof(float);
                new (ptr) float(v);
            },
            [&](const uint32_t &v) {
                entry.size = sizeof(uint32_t);
                new (ptr) uint32_t(v);
            },
            [&](const int32_t &v) {
                entry.size = sizeof(int32_t);
                new (ptr) int32_t(v);
            }
        }, value);

        currentSize += entry.size;
        SKY_ASSERT(currentSize <= storage.size());
        properties[name] = entry;
    }

    void MaterialPropertyInitializer::Finalize()
    {
        storage.resize(currentSize);
        storage.shrink_to_fit();
    }

    void Material::SetProperties(MaterialPropertyInitializer &&initializer)
    {
        properties = std::move(initializer.properties);
        storage = std::move(initializer.storage);
        textures = std::move(initializer.textures);
    }

    void Material::AddTechnique(const RDGfxTechPtr &technique)
    {
        gfxTechniques.emplace_back(technique);
    }

    void MaterialInstance::SetMaterial(const RDMaterialPtr &mat)
    {
        material = mat;
        storage = std::make_unique<uint8_t []>(mat->storage.size());
        memcpy(storage.get(), mat->storage.data(), mat->storage.size());

        textures = mat->textures;
    }

    void MaterialInstance::SetTexture(const Name &key, const RDTexturePtr &tex)
    {
        auto iter = material->properties.find(key);
        if (iter != material->properties.end() && textures[iter->second.offset].Get() != tex.Get()) {
            textures[iter->second.offset] = tex;
            ++batchVersion;
        }
    }

    void MaterialInstance::Upload()
    {
        if (uploadVersion != batchVersion) {
            auto *sm = Renderer::Get()->GetStreamingManager();
            for (const auto &tex : textures) {
                sm->UploadTexture(tex);
            }
            uploadVersion = batchVersion;
        }
    }

    bool MaterialInstance::IsReady() const
    {
        return !std::any_of(textures.begin(), textures.end(), [](const auto &tex) { return !tex->IsReady(); });
    }

} // namespace sky
