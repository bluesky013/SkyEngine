//
// Material: inline-technique material model. Material owns a list of
// techniques (tag + shader + PipelineState, i.e. the old "technique" is
// inlined rather than a top-level resource). MaterialInstance shares a
// Material and sparsely overrides a subset of its property fields at runtime.
//

#pragma once

#include <aurora/resource/Texture.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Shader.h>
#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sky::aurora {

    // One render technique of a material: a tag (for RDG queue bucketing), a
    // shader and the render state. Replaces the old top-level Technique +
    // RenderTechniqueLibrary (the "state" and "techID" roles are inlined).
    struct MaterialTechnique {
        Name               techniqueTag;
        CounterPtr<Shader> shader;
        PipelineState      state;
    };

    enum class MaterialPropertyType : uint8_t {
        VALUE = 0,
        TEXTURE,
    };

    struct MaterialPropertyEntry {
        MaterialPropertyType type = MaterialPropertyType::VALUE;
        uint32_t offset = 0;   // VALUE: byte offset into storage; TEXTURE: index into textures
        uint32_t size   = 0;   // VALUE: byte size
    };

    using MaterialPropertyMap = std::unordered_map<Name, MaterialPropertyEntry>;

    class Material : public RefObject {
    public:
        Material() = default;
        ~Material() override = default;

        Material(const Material &) = delete;
        Material &operator=(const Material &) = delete;

        // ---- techniques (inlined) ----
        void AddTechnique(const MaterialTechnique &technique)
        {
            techniques.push_back(technique);
        }

        const std::vector<MaterialTechnique> &GetTechniques() const { return techniques; }

        const MaterialTechnique *GetTechnique(const Name &tag) const
        {
            for (const auto &technique : techniques) {
                if (technique.techniqueTag == tag) {
                    return &technique;
                }
            }
            return nullptr;
        }

        // ---- property layout ----
        void AddValue(const Name &name, uint32_t size, const void *defaultValue)
        {
            MaterialPropertyEntry entry;
            entry.type   = MaterialPropertyType::VALUE;
            entry.offset = static_cast<uint32_t>(storage.size());
            entry.size   = size;
            storage.resize(storage.size() + size, 0);
            if (defaultValue != nullptr) {
                std::memcpy(storage.data() + entry.offset, defaultValue, size);
            }
            properties[name] = entry;
        }

        void AddTexture(const Name &name)
        {
            MaterialPropertyEntry entry;
            entry.type   = MaterialPropertyType::TEXTURE;
            entry.offset = static_cast<uint32_t>(textures.size());
            textures.emplace_back();
            properties[name] = entry;
        }

        const MaterialPropertyMap &GetPropertyMap() const { return properties; }

        // ---- property values ----
        template <typename T>
        void SetValue(const Name &name, const T &value)
        {
            MaterialPropertyEntry *entry = FindValue(name, sizeof(T));
            if (entry != nullptr) {
                std::memcpy(storage.data() + entry->offset, &value, sizeof(T));
            }
        }

        template <typename T>
        bool GetValue(const Name &name, T &out) const
        {
            const MaterialPropertyEntry *entry = FindValue(name, sizeof(T));
            if (entry == nullptr) {
                return false;
            }
            std::memcpy(&out, storage.data() + entry->offset, sizeof(T));
            return true;
        }

        void SetTexture(const Name &name, CounterPtr<Texture> texture)
        {
            const MaterialPropertyEntry *entry = FindTexture(name);
            if (entry != nullptr) {
                textures[entry->offset] = std::move(texture);
            }
        }

        Texture *GetTexture(const Name &name) const
        {
            const MaterialPropertyEntry *entry = FindTexture(name);
            return entry != nullptr ? textures[entry->offset].Get() : nullptr;
        }

    private:
        friend class MaterialInstance;

        MaterialPropertyEntry *FindValue(const Name &name, uint32_t size)
        {
            auto iter = properties.find(name);
            if (iter == properties.end() ||
                iter->second.type != MaterialPropertyType::VALUE ||
                iter->second.size != size) {
                return nullptr;
            }
            return &iter->second;
        }

        const MaterialPropertyEntry *FindValue(const Name &name, uint32_t size) const
        {
            auto iter = properties.find(name);
            if (iter == properties.end() ||
                iter->second.type != MaterialPropertyType::VALUE ||
                iter->second.size != size) {
                return nullptr;
            }
            return &iter->second;
        }

        const MaterialPropertyEntry *FindTexture(const Name &name) const
        {
            auto iter = properties.find(name);
            if (iter == properties.end() || iter->second.type != MaterialPropertyType::TEXTURE) {
                return nullptr;
            }
            return &iter->second;
        }

        std::vector<MaterialTechnique>   techniques;
        std::vector<uint8_t>             storage;
        MaterialPropertyMap              properties;
        std::vector<CounterPtr<Texture>> textures;
    };

    // Shares a Material and overrides a subset of its fields at runtime. Only
    // the overridden fields are stored; everything else inherits the material.
    class MaterialInstance : public RefObject {
    public:
        MaterialInstance() = default;
        ~MaterialInstance() override = default;

        MaterialInstance(const MaterialInstance &) = delete;
        MaterialInstance &operator=(const MaterialInstance &) = delete;

        void SetMaterial(CounterPtr<Material> inMaterial) { material = std::move(inMaterial); }
        Material *GetMaterial() const { return material.Get(); }

        template <typename T>
        void SetValue(const Name &name, const T &value)
        {
            if (material == nullptr || material->FindValue(name, sizeof(T)) == nullptr) {
                return;
            }
            std::vector<uint8_t> bytes(sizeof(T));
            std::memcpy(bytes.data(), &value, sizeof(T));
            valueOverrides[name] = std::move(bytes);
        }

        template <typename T>
        bool GetValue(const Name &name, T &out) const
        {
            auto iter = valueOverrides.find(name);
            if (iter != valueOverrides.end() && iter->second.size() == sizeof(T)) {
                std::memcpy(&out, iter->second.data(), sizeof(T));
                return true;
            }
            return material != nullptr ? material->GetValue(name, out) : false;
        }

        void SetTexture(const Name &name, CounterPtr<Texture> texture)
        {
            if (material == nullptr || material->FindTexture(name) == nullptr) {
                return;
            }
            textureOverrides[name] = std::move(texture);
        }

        Texture *GetTexture(const Name &name) const
        {
            auto iter = textureOverrides.find(name);
            if (iter != textureOverrides.end()) {
                return iter->second.Get();
            }
            return material != nullptr ? material->GetTexture(name) : nullptr;
        }

        bool IsOverridden(const Name &name) const
        {
            return valueOverrides.find(name) != valueOverrides.end() ||
                   textureOverrides.find(name) != textureOverrides.end();
        }

    private:
        CounterPtr<Material> material;
        std::unordered_map<Name, std::vector<uint8_t>> valueOverrides;
        std::unordered_map<Name, CounterPtr<Texture>>  textureOverrides;
    };

} // namespace sky::aurora
