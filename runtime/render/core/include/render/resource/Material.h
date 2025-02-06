//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <unordered_map>
#include <render/RenderResource.h>
#include <render/resource/Texture.h>
#include <render/resource/Technique.h>
#include <render/resource/ResourceGroup.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>

namespace sky {

    struct MaterialTexture {
        Uuid texID;
    };

    struct TextureSampler {
        uint8_t magFilter = 0;
        uint8_t minFilter = 0;
        uint8_t mipMode = 0;
        uint8_t addressModeU = 0;
        uint8_t addressModeV = 0;
        uint8_t addressModeW = 0;
        uint8_t rsv[2] = {};

        bool operator == (const TextureSampler &rhs) const
        {
            return magFilter == rhs.magFilter &&
                minFilter == rhs.minFilter &&
                mipMode == rhs.mipMode &&
                addressModeU == rhs.addressModeU &&
                addressModeV == rhs.addressModeV &&
                addressModeW == rhs.addressModeW;
        }

        bool operator != (const TextureSampler &rhs) const
        {
            return magFilter != rhs.magFilter ||
                minFilter != rhs.minFilter ||
                mipMode != rhs.mipMode ||
                addressModeU != rhs.addressModeU ||
                addressModeV != rhs.addressModeV ||
                addressModeW != rhs.addressModeW;
        }
    };

    enum class MaterialValueType : uint16_t {
        TEXTURE,
        SAMPLER,
        VEC2,
        VEC3,
        VEC4,
        FLOAT,
        U32,
        I32
    };

    using MaterialValue = std::variant<MaterialTexture, TextureSampler, Vector2, Vector3, Vector4, float, uint32_t, int32_t>;

    struct MaterialPropertyEntry {
        uint16_t offset;
        uint16_t size;
    };

    struct MaterialPropertyInitializer {

        explicit MaterialPropertyInitializer(size_t propertyNum);

        void AddPropertyValue(const Name& name, const MaterialValue &val);
        void AddPropertyTexture(const Name& name, const RDTexturePtr &tex);
        void Finalize();

        std::vector<uint8_t> storage;
        std::unordered_map<Name, MaterialPropertyEntry> properties;
        std::vector<RDTexturePtr> textures;
        uint32_t currentSize = 0;
    };

    using MaterialPropertyMap = std::unordered_map<Name, MaterialPropertyEntry>;

    class Material : public RenderResource {
    public:
        Material()  = default;
        ~Material() override = default;

        void SetProperties(MaterialPropertyInitializer &&initializer);

        void AddTechnique(const RDGfxTechPtr &technique);
        const std::vector<RDGfxTechPtr> &GetGfxTechniques() const { return gfxTechniques; }
        const MaterialPropertyMap &GetPropertyMap() const { return properties; }
    private:
        friend class MaterialInstance;

        std::vector<RDGfxTechPtr> gfxTechniques;
        std::vector<uint8_t> storage;
        MaterialPropertyMap properties;
        std::vector<RDTexturePtr> textures;
    };

    using RDMaterialPtr = CounterPtr<Material>;

    class MaterialInstance : public RenderResource {
    public:
        MaterialInstance() = default;
        ~MaterialInstance() override = default;

        void SetMaterial(const RDMaterialPtr &mat);

        void SetTexture(const Name &key, const RDTexturePtr &tex);

        template <class T>
        void SetValue(const Name &key, const T &val)
        {
            auto iter = material->properties.find(key);
            if (iter != material->properties.end()) {
                auto *ptr = storage.get() + iter->second.offset;
                memcpy(ptr, &val, iter->second.size);
                ++valueVersion;
            }
        }

        template <class T>
        const T* GetValue(const Name& key) const
        {
            auto iter = material->properties.find(key);
            if (iter != material->properties.end()) {
                return reinterpret_cast<T*>(storage.get() + iter->second.offset);
            }
            return nullptr;
        }

        void GetValueRaw(const Name& key, uint8_t* ptr, uint32_t size)
        {
            auto iter = material->properties.find(key);
            if (iter != material->properties.end()) {
                const uint8_t *src = storage.get() + iter->second.offset;
                memcpy(ptr, src, iter->second.size);
            }
        }

        const RDTexturePtr &GetTexture(const Name& key) const
        {
            static const RDTexturePtr EMPTY = {};
            auto iter = material->properties.find(key);
            if (iter != material->properties.end()) {
                return textures[iter->second.offset];
            }
            return EMPTY;
        }

        const RDMaterialPtr &GetMaterial() const { return material; }
        const MaterialPropertyMap &GetPropertyMap() const { return material->GetPropertyMap(); }

        inline uint16_t GetValueVersion() const { return valueVersion; }
        inline uint16_t GetBatchVersion() const { return batchVersion; }

        void UploadTextures();
        bool IsReady() const;
    private:
        RDMaterialPtr material;
        std::unique_ptr<uint8_t []> storage;
        std::vector<RDTexturePtr> textures;

        uint16_t valueVersion = 0;
        uint16_t batchVersion = 0;
        uint16_t uploadVersion = -1;
    };

    using RDMaterialInstancePtr = CounterPtr<MaterialInstance>;
} // namespace sky