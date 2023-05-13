//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <variant>
#include <memory>
#include <unordered_map>
#include <string>
#include <core/math/Matrix4.h>
#include <rhi/ImageView.h>
#include <rhi/Sampler.h>
#include <rhi/DescriptorSet.h>
#include <rhi/Buffer.h>

namespace sky::rhi {
    struct Texture {
        ImageViewPtr view;
        SamplerPtr sampler;
    };

    class Material {
    public:
        Material() = default;
        ~Material() = default;

        struct Accessor {
            uint32_t binding = 0;
            uint32_t offset  = 0;
        };

        struct Connection {
            std::unordered_map<std::string, Accessor> accessors;
        };

        void SetLayout(const rhi::DescriptorSetLayoutPtr &layout, uint32_t size);
        void AddConnection(const std::string &str, const Accessor &accessor);

        template <typename T>
        void SetValue(const std::string &key, const T &value)
        {
            SetValue(key, reinterpret_cast<const uint8_t *>(&value), sizeof(T));
        }
        void SetValue(const std::string &str, const uint8_t *data, uint32_t size);
        void SetTexture(const std::string &key, const Texture &tex);
        void Update();

        const DescriptorSetLayoutPtr &GetLayout() const { return layout; }
        const DescriptorSetPtr &GetSet() const { return batchSet; }

    private:
        Accessor GetAccessor(const std::string &key) const;
        using RawData = std::vector<uint8_t>;

        DescriptorSetPtr batchSet;
        DescriptorSetLayoutPtr layout;
        Connection connection;

        uint32_t bufferBinding = 0;
        std::unordered_map<uint32_t, Texture> textures;
        std::unordered_map<uint32_t, RawData> rawDatas;
        std::unordered_map<uint32_t, BufferPtr> buffers;

        bool needUpdateSet = true;
    };
    using MaterialPtr = std::shared_ptr<Material>;

}