//
// Material owns tagged programs and authored properties; instances share
// the definition while retaining their own property overrides.
//

#pragma once

#include <aurora/resource/Texture.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Sampler.h>
#include <aurora/rhi/ShaderParameters.h>
#include <aurora/shader/RgBlockDesc.h>
#include <aurora/shader/ShaderTechnique.h>
#include <cmath>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/name/Name.h>
#include <core/platform/Platform.h>
#include <core/template/ReferenceObject.h>
#include <type_traits>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sky::aurora {

    // Property types reuse the shader field vocabulary (RgFieldType) so the
    // material and its shader blocks share a single type definition.
    using MaterialPropertyType = RgFieldType;

    constexpr bool IsTextureProperty(MaterialPropertyType type)
    {
        return type == MaterialPropertyType::TEXTURE2D || type == MaterialPropertyType::TEXTURE_CUBE;
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

    template <class T>
    constexpr MaterialPropertyType MaterialPropertyTypeOf()
    {
        if constexpr (std::is_same_v<T, float>)
            return MaterialPropertyType::FLOAT;
        else if constexpr (std::is_same_v<T, Vector2>)
            return MaterialPropertyType::FLOAT2;
        else if constexpr (std::is_same_v<T, Vector3>)
            return MaterialPropertyType::FLOAT3;
        else if constexpr (std::is_same_v<T, Vector4>)
            return MaterialPropertyType::FLOAT4;
        else if constexpr (std::is_same_v<T, Matrix4>)
            return MaterialPropertyType::MAT4;
        else if constexpr (std::is_same_v<T, int32_t>)
            return MaterialPropertyType::INT;
        else if constexpr (std::is_same_v<T, uint32_t>)
            return MaterialPropertyType::UINT;
        else if constexpr (std::is_same_v<T, bool>)
            return MaterialPropertyType::BOOL;
        else
            return static_cast<MaterialPropertyType>(UINT8_MAX);
    }

    struct MaterialTechnique {
        Name                              techniqueTag;
        ShaderTechnique                   program;
        std::vector<ShaderParameterAlias> aliases;
        MaterialRenderState               renderState;
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
        bool AddTechnique(const MaterialTechnique &technique)
        {
            if (technique.techniqueTag.Empty() || GetTechnique(technique.techniqueTag))
                return false;
            techniques.push_back(technique);
            return true;
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
        bool AddValue(const Name &name, MaterialPropertyType type, std::nullptr_t = nullptr)
        {
            return AddValueStorage(name, type, nullptr);
        }
        template <class T>
        bool AddValue(const Name &name, MaterialPropertyType type, const T *value)
        {
            return MaterialPropertyTypeOf<T>() == type && AddValueStorage(name, type, value);
        }
        bool AddTexture(const Name &name, MaterialPropertyType type = MaterialPropertyType::TEXTURE2D)
        {
            if (name.Empty() || properties.contains(name) || !IsTextureProperty(type) || textures.size() >= UINT32_MAX)
                return false;
            properties.emplace(name, MaterialPropertyEntry{type, static_cast<uint32_t>(textures.size())});
            textures.emplace_back();
            return true;
        }
        bool AddSampler(const Name &name, const Sampler::Descriptor &value = {})
        {
            if (name.Empty() || properties.contains(name) || !IsValidMaterialSampler(value) || samplers.size() >= UINT32_MAX)
                return false;
            properties.emplace(name, MaterialPropertyEntry{MaterialPropertyType::SAMPLER, static_cast<uint32_t>(samplers.size())});
            samplers.push_back(value);
            return true;
        }

        const MaterialPropertyMap &GetPropertyMap() const
        {
            return properties;
        }

        template <typename T>
        bool SetValue(const Name &name, const T &value)
        {
            const auto *entry = FindValue(name, MaterialPropertyTypeOf<T>());
            if (!entry)
                return false;
            std::memcpy(storage.data() + entry->offset, &value, sizeof(T));
            return true;
        }

        template <typename T>
        bool GetValue(const Name &name, T &out) const
        {
            const MaterialPropertyEntry *entry = FindValue(name, MaterialPropertyTypeOf<T>());
            if (entry == nullptr) {
                return false;
            }
            std::memcpy(&out, storage.data() + entry->offset, sizeof(T));
            return true;
        }

        bool GetData(const Name &name, ShaderDataValue &out) const
        {
            const auto *entry = Find(name);
            return entry && ReadShaderData(*entry, storage.data(), out);
        }

        bool SetTexture(const Name &name, CounterPtr<Texture> texture)
        {
            const auto *entry = FindTexture(name);
            if (!entry || !TextureMatches(entry->type, texture.Get()))
                return false;
            textures[entry->offset] = std::move(texture);
            return true;
        }
        bool SetSampler(const Name &name, const Sampler::Descriptor &value)
        {
            const auto *entry = Find(name);
            if (!entry || entry->type != MaterialPropertyType::SAMPLER || !IsValidMaterialSampler(value))
                return false;
            samplers[entry->offset] = value;
            return true;
        }
        bool GetSampler(const Name &name, Sampler::Descriptor &out) const
        {
            const auto *entry = Find(name);
            if (!entry || entry->type != MaterialPropertyType::SAMPLER)
                return false;
            out = samplers[entry->offset];
            return true;
        }

        Texture *GetTexture(const Name &name) const
        {
            const MaterialPropertyEntry *entry = FindTexture(name);
            return entry != nullptr ? textures[entry->offset].Get() : nullptr;
        }

    private:
        friend class MaterialInstance;

        static bool ReadShaderData(const MaterialPropertyEntry &entry, const uint8_t *source, ShaderDataValue &out)
        {
            const auto read = [&]<class T>() {
                T value;
                std::memcpy(&value, source + entry.offset, sizeof(T));
                out = MakeShaderData(value);
                return true;
            };
            switch (entry.type) {
            case MaterialPropertyType::FLOAT: return read.template operator()<float>();
            case MaterialPropertyType::FLOAT2: return read.template operator()<Vector2>();
            case MaterialPropertyType::FLOAT4: return read.template operator()<Vector4>();
            case MaterialPropertyType::MAT4: return read.template operator()<Matrix4>();
            case MaterialPropertyType::INT: return read.template operator()<int32_t>();
            case MaterialPropertyType::UINT: return read.template operator()<uint32_t>();
            default: return false;
            }
        }

        bool AddValueStorage(const Name &name, MaterialPropertyType type, const void *value)
        {
            const auto size = SizeOfPropertyType(type);
            if (name.Empty() || properties.contains(name) || !size || storage.size() > UINT32_MAX - size)
                return false;
            const auto offset = static_cast<uint32_t>(storage.size());
            storage.resize(storage.size() + size, 0);
            if (value)
                std::memcpy(storage.data() + offset, value, size);
            properties.emplace(name, MaterialPropertyEntry{type, offset});
            return true;
        }
        static bool TextureMatches(MaterialPropertyType type, const Texture *texture)
        {
            if (!texture)
                return true;
            const auto &desc = texture->GetDescriptor();
            return desc.imageType == ImageType::IMAGE_2D && (type == MaterialPropertyType::TEXTURE2D ? desc.arrayLayers == 1 : desc.arrayLayers == 6);
        }
        const MaterialPropertyEntry *Find(const Name &name) const
        {
            auto iter = properties.find(name);
            return iter == properties.end() ? nullptr : &iter->second;
        }

        const MaterialPropertyEntry *FindValue(const Name &name, MaterialPropertyType type) const
        {
            const MaterialPropertyEntry *entry = Find(name);
            if (entry == nullptr || !SizeOfPropertyType(type) || entry->type != type) {
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
        std::vector<Sampler::Descriptor> samplers;
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
            valueOverrides.clear();
            textureOverrides.clear();
            samplerOverrides.clear();
        }

        Material *GetMaterial() const
        {
            return material.Get();
        }

        template <typename T>
        bool SetValue(const Name &name, const T &value)
        {
            if (material == nullptr) {
                return false;
            }
            const MaterialPropertyEntry *entry = material->FindValue(name, MaterialPropertyTypeOf<T>());
            if (entry == nullptr) {
                return false;
            }
            EnsureStorage();
            std::memcpy(storage.data() + entry->offset, &value, sizeof(T));
            valueOverrides.insert(name);
            return true;
        }

        template <typename T>
        bool GetValue(const Name &name, T &out) const
        {
            if (material == nullptr) {
                return false;
            }
            const MaterialPropertyEntry *entry = material->FindValue(name, MaterialPropertyTypeOf<T>());
            if (entry == nullptr) {
                return false;
            }
            const uint8_t *src = valueOverrides.contains(name) ? storage.data() : material->storage.data();
            std::memcpy(&out, src + entry->offset, sizeof(T));
            return true;
        }

        bool GetData(const Name &name, ShaderDataValue &out) const
        {
            const auto *entry  = material ? material->Find(name) : nullptr;
            const auto *source = valueOverrides.contains(name) ? storage.data() : material ? material->storage.data() : nullptr;
            return entry && Material::ReadShaderData(*entry, source, out);
        }

        bool SetTexture(const Name &name, CounterPtr<Texture> texture)
        {
            const auto *entry = material ? material->FindTexture(name) : nullptr;
            if (!entry || !Material::TextureMatches(entry->type, texture.Get()))
                return false;
            textureOverrides[name] = std::move(texture);
            return true;
        }
        bool SetSampler(const Name &name, const Sampler::Descriptor &value)
        {
            const auto *entry = material ? material->Find(name) : nullptr;
            if (!entry || entry->type != MaterialPropertyType::SAMPLER || !IsValidMaterialSampler(value))
                return false;
            samplerOverrides[name] = value;
            return true;
        }
        bool GetSampler(const Name &name, Sampler::Descriptor &out) const
        {
            const auto found = samplerOverrides.find(name);
            if (found != samplerOverrides.end()) {
                out = found->second;
                return true;
            }
            return material && material->GetSampler(name, out);
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
            return material && (valueOverrides.contains(name) || textureOverrides.contains(name) || samplerOverrides.contains(name));
        }

    private:
        void EnsureStorage()
        {
            if (storage.size() != material->storage.size()) {
                auto extended = material->storage;
                std::copy(storage.begin(), storage.end(), extended.begin());
                storage = std::move(extended);
            }
        }

        std::unordered_set<Name>                      valueOverrides;
        CounterPtr<Material>                          material;
        std::vector<uint8_t>                          storage;
        std::unordered_map<Name, CounterPtr<Texture>> textureOverrides;
        std::unordered_map<Name, Sampler::Descriptor> samplerOverrides;
    };

} // namespace sky::aurora
