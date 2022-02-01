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
#include <vulkan/Buffer.h>
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
        void serialize(Archive& ar)
        {
            ar(meshes);
        }
    };

    struct BufferView {
        drv::BufferPtr buffer;
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
        static constexpr Uuid TYPE = Uuid::CreateFromString("9f7c599a-0073-4ff5-8136-f551d1a1a371");

        MeshAsset(const Uuid& id) : AssetBase(id) {}
        ~MeshAsset() = default;

        template<class Archive>
        void load(Archive& ar)
        {
            ar(data);
        }

        template<class Archive>
        void save(Archive& ar) const
        {
            ar(data);
        }

        const Uuid& GetType() const override { return TYPE; }

        MeshData data;
    };

    using MeshAssetPtr = CounterPtr<MeshAsset>;

    struct Primitive {
        uint32_t vertices;
        std::vector<BufferView> vertexBuffers;
        std::vector<Attribute> attributes;
        BufferView indices;
        MaterialPtr material;
        AABB aabb;
    };

    class Mesh : public ResourceBase {
    public:
        Mesh(const Uuid& id) : ResourceBase(id) {}
        ~Mesh() = default;

        static CounterPtr<Mesh> CreateFromAsset(AssetPtr asset);

    private:
        drv::BufferPtr vertexBuffer;
        drv::BufferPtr indexBuffer;
        std::vector<Primitive> primitives;
    };

    using MeshPtr = CounterPtr<Mesh>;

}