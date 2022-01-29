//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>
#include <core/math/Vector.h>
#include <engine/asset/BufferAsset.h>
#include <engine/asset/MaterialAsset.h>
#include <vulkan/Device.h>
#include <vector>

namespace sky {

    struct AABB {
        Vector3 min;
        Vector3 max;
    };

    struct Vertex {
        Vector4 pos    = { 0.f, 0.f, 0.f, 1.f};
        Vector4 normal = {1.f, 0.f, 0.f, 1.f};
        Vector4 color  = {1.f, 1.f, 1.f, 1.f};

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(pos, normal, color);
        }
    };

    struct SubMeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        AABB aabb;

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(vertices, indices, aabb.min, aabb.max);
        }
    };

    struct MeshData {
        std::vector<SubMeshData> meshes;

        template <class Archive>
        void save(Archive& ar) const
        {
            ar(meshes);
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(meshes);
        }
    };

    struct BufferView {
        BufferPtr buffer;
        uint32_t offset = 0;
        uint32_t stride = 0;
    };

    struct Attribute {
        uint32_t binding = 0;
        uint32_t offset = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    class MeshAsset : public AssetBase {
    public:
        MeshAsset(const Uuid& id) : AssetBase(id) {}
        ~MeshAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("9f7c599a-0073-4ff5-8136-f551d1a1a371");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    class MeshAssetHandler : public AssetHandlerBase {
    public:
        MeshAssetHandler() = default;
        virtual ~MeshAssetHandler() = default;

        AssetBase* Create(const Uuid& id) override;

        AssetBase* Load(const std::string&) override;
    };

    class Mesh : public ResourceBase {
    public:
        Mesh(const Uuid& id) : ResourceBase(id) {}
        ~Mesh() = default;

    private:
        uint32_t vertices;
        std::vector<BufferView> vertexBuffers;
        BufferView indices;
        std::vector<Attribute> attributes;
        MaterialPtr material;
        AABB aabb;
    };

}