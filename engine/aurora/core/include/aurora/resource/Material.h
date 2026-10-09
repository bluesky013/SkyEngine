//
// Material: inline-technique material model. Material owns a list of
// techniques (tag + shader + MaterialRenderState, i.e. the old "technique" is
// inlined rather than a top-level resource) plus material-level properties
// (values + textures). MaterialInstance shares a Material and overrides a
// subset of its fields at runtime, holding its own copy of the value block.
//

#pragma once

#include <aurora/resource/Texture.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Shader.h>
#include <aurora/shader/RgBlockDesc.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/name/Name.h>
#include <core/platform/Platform.h>
#include <core/template/ReferenceObject.h>

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sky::aurora {

    // Property types reuse the shader field vocabulary (RgFieldType) so the
    // material and its shader blocks share a single type definition.
    using MaterialPropertyType = RgFieldType;

    constexpr bool IsTextureProperty(MaterialPropertyType type)
    {
        return type == MaterialPropertyType::TEXTURE2D || type == MaterialPropertyType::TEXTURE_CUBE || type == MaterialPropertyType::SAMPLER;
    }

    // Byte size implied by a property type (packed CPU layout); callers never
    // supply a size, it is derived from the type.
    constexpr uint32_t SizeOfPropertyType(MaterialPropertyType type)
    {
        switch (type) {
        case MaterialPropertyType::FLOAT: return 4;
        case MaterialPropertyType::FLOAT2: return 8;
        case MaterialPropertyType::FLOAT3: return 12;
        case MaterialPropertyType::FLOAT4: return 16;
        case MaterialPropertyType::MAT4: return 64;
        case MaterialPropertyType::INT: return 4;
        case MaterialPropertyType::UINT: return 4;
        case MaterialPropertyType::BOOL: return 1;
        default: return 0;
        }
    }

    // Material-intrinsic render state: the fixed-function state owned by the
    // material, not by the mesh or the draw pass. Mesh-owned vertex input /
    // topology and pass-owned sample count live outside the material; the
    // renderer merges them into the RHI PipelineState at pipeline creation.
    struct MaterialRenderState {
        DepthStencil            depthStencil;
        RasterState             rasterState;
        std::vector<BlendState> blendStates; // per color target
        bool                    alphaToCoverage = false;
    };

    // One render technique of a material: a tag (for RDG queue bucketing), a
    // shader and the material render state. Replaces the old top-level
    // Technique + RenderTechniqueLibrary (the "state" and "techID" roles are
    // inlined).
    struct MaterialTechnique {
        Name                techniqueTag;
        CounterPtr<Shader>  shader;
        MaterialRenderState renderState;
    };

    struct MaterialPropertyEntry {
        MaterialPropertyType type   = MaterialPropertyType::FLOAT;
        uint32_t             offset = 0; // value: byte offset into storage; texture: index into textures
    };

    using MaterialPropertyMap = std::unordered_map<Name, MaterialPropertyEntry>;

    class Material : public RefObject {
    public:
        Material()           = default;
        ~Material() override = default;

        Material(const Material &)            = delete;
        Material &operator=(const Material &) = delete;

        // ---- techniques (inlined) ----
        void AddTechnique(const MaterialTechnique &technique)
        {
            techniques.push_back(technique);
        }

        const std::vector<MaterialTechnique> &GetTechniques() const
        {
            return techniques;
        }

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
        void AddValue(const Name &name, MaterialPropertyType type, const void *defaultValue)
        {
            SKY_ASSERT(!IsTextureProperty(type));
            const uint32_t        size = SizeOfPropertyType(type);
            MaterialPropertyEntry entry;
            entry.type   = type;
            entry.offset = static_cast<uint32_t>(storage.size());
            storage.resize(storage.size() + size, 0);
            if (defaultValue != nullptr) {
                std::memcpy(storage.data() + entry.offset, defaultValue, size);
            }
            properties[name] = entry;
        }

        void AddTexture(const Name &name, MaterialPropertyType type = MaterialPropertyType::TEXTURE2D)
        {
            SKY_ASSERT(IsTextureProperty(type));
            MaterialPropertyEntry entry;
            entry.type   = type;
            entry.offset = static_cast<uint32_t>(textures.size());
            textures.emplace_back();
            properties[name] = entry;
        }

        const MaterialPropertyMap &GetPropertyMap() const
        {
            return properties;
        }

        // ---- property values ----
        // Type is validated by size against the declared property: a value whose
        // sizeof does not match the declared type is rejected.
        template <typename T>
        void SetValue(const Name &name, const T &value)
        {
            const MaterialPropertyEntry *entry = FindValue(name, sizeof(T));
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

        const MaterialPropertyEntry *Find(const Name &name) const
        {
            auto iter = properties.find(name);
            return iter == properties.end() ? nullptr : &iter->second;
        }

        const MaterialPropertyEntry *FindValue(const Name &name, uint32_t size) const
        {
            const MaterialPropertyEntry *entry = Find(name);
            if (entry == nullptr || IsTextureProperty(entry->type) || SizeOfPropertyType(entry->type) != size) {
                return nullptr;
            }
            return entry;
        }

        const MaterialPropertyEntry *FindTexture(const Name &name) const
        {
            const MaterialPropertyEntry *entry = Find(name);
            return entry != nullptr && IsTextureProperty(entry->type) ? entry : nullptr;
        }

        std::vector<MaterialTechnique>   techniques;
        std::vector<uint8_t>             storage;
        MaterialPropertyMap              properties;
        std::vector<CounterPtr<Texture>> textures;
    };

    // Shares a Material and overrides a subset of its fields at runtime. Value
    // overrides live in a single contiguous copy of the material's storage
    // (lazily copied on first write); texture overrides are a sparse map.
    class MaterialInstance : public RefObject {
    public:
        MaterialInstance()           = default;
        ~MaterialInstance() override = default;

        MaterialInstance(const MaterialInstance &)            = delete;
        MaterialInstance &operator=(const MaterialInstance &) = delete;

        void SetMaterial(CounterPtr<Material> inMaterial)
        {
            material = std::move(inMaterial);
            storage.clear();
            textureOverrides.clear();
        }

        Material *GetMaterial() const
        {
            return material.Get();
        }

        template <typename T>
        void SetValue(const Name &name, const T &value)
        {
            if (material == nullptr) {
                return;
            }
            const MaterialPropertyEntry *entry = material->FindValue(name, sizeof(T));
            if (entry == nullptr) {
                return;
            }
            EnsureStorage();
            std::memcpy(storage.data() + entry->offset, &value, sizeof(T));
        }

        template <typename T>
        bool GetValue(const Name &name, T &out) const
        {
            if (material == nullptr) {
                return false;
            }
            const MaterialPropertyEntry *entry = material->FindValue(name, sizeof(T));
            if (entry == nullptr) {
                return false;
            }
            const uint8_t *src = storage.empty() ? material->storage.data() : storage.data();
            std::memcpy(&out, src + entry->offset, sizeof(T));
            return true;
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

        // A field is overridden when it differs from the material default.
        bool IsOverridden(const Name &name) const
        {
            if (material == nullptr) {
                return false;
            }
            if (textureOverrides.find(name) != textureOverrides.end()) {
                return true;
            }
            const MaterialPropertyEntry *entry = material->Find(name);
            if (entry == nullptr || IsTextureProperty(entry->type) || storage.empty()) {
                return false;
            }
            const uint32_t size = SizeOfPropertyType(entry->type);
            return std::memcmp(storage.data() + entry->offset, material->storage.data() + entry->offset, size) != 0;
        }

    private:
        void EnsureStorage()
        {
            if (storage.size() != material->storage.size()) {
                storage = material->storage;
            }
        }

        CounterPtr<Material>                          material;
        std::vector<uint8_t>                          storage;
        std::unordered_map<Name, CounterPtr<Texture>> textureOverrides;
    };

} // namespace sky::aurora
