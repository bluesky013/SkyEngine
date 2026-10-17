#pragma once

#include <algorithm>
#include <aurora/renderer/RenderRasterData.h>
#include <aurora/renderer/RendererTypes.h>
#include <bit>
#include <cmath>
#include <core/name/Name.h>
#include <core/shapes/AABB.h>
#include <core/util/Uuid.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace sky::aurora {

    struct RenderResourceKey {
        Uuid     product;
        uint64_t revision                                    = 0;
        uint64_t usageVariant                                = 0;
        uint64_t deviceEpoch                                 = 0;
        bool     operator==(const RenderResourceKey &) const = default;
    };

    struct RenderResourceBinding {
        RenderResourceKey resource;
        uint64_t          bindingRevision = 0;
    };

    enum class RenderImageFormat : uint8_t { RGBA8_UNORM };
    struct RenderImageLayout {
        uint32_t          width = 0, height = 0, mipLevels = 1, arrayLayers = 1;
        RenderImageFormat format = RenderImageFormat::RGBA8_UNORM;
    };
    // Complete 2D payloads are tightly packed, layer-major then mip-major, with no row padding.
    inline std::optional<uint64_t> RenderImageByteSize(const RenderImageLayout &image)
    {
        if (!image.width || !image.height || !image.arrayLayers || !image.mipLevels ||
            image.mipLevels > std::bit_width(std::max(image.width, image.height)) || image.format != RenderImageFormat::RGBA8_UNORM)
            return {};
        uint64_t bytes = 0;
        for (uint32_t mip = 0; mip < image.mipLevels; ++mip) {
            const uint64_t pixels = uint64_t(std::max(1u, image.width >> mip)) * std::max(1u, image.height >> mip);
            if (pixels > (UINT64_MAX - bytes) / 4)
                return {};
            bytes += pixels * 4;
        }
        if (bytes > UINT64_MAX / image.arrayLayers)
            return {};
        return bytes * image.arrayLayers;
    }
    struct RenderResourceMetadata {
        std::optional<AABB>              bounds;
        std::vector<float>               lodDistances;
        std::optional<RenderImageLayout> image;
    };

    inline bool IsValidRenderResourceMetadata(const RenderResourceMetadata &metadata)
    {
        if (metadata.lodDistances.size() > 7 || (metadata.image && !RenderImageByteSize(*metadata.image)))
            return false;
        float previous = 0;
        for (float distance : metadata.lodDistances) {
            if (!std::isfinite(distance) || distance <= previous)
                return false;
            previous = distance;
        }
        if (!metadata.bounds)
            return true;
        const auto &box = *metadata.bounds;
        return std::isfinite(box.min.x) && std::isfinite(box.min.y) && std::isfinite(box.min.z) && std::isfinite(box.max.x) &&
               std::isfinite(box.max.y) && std::isfinite(box.max.z) && box.min.x <= box.max.x && box.min.y <= box.max.y && box.min.z <= box.max.z;
    }

    enum class ResourceBindingSlot : uint8_t { MESH, MATERIAL };
    struct ResourceSubscriber {
        RenderEntityRef     entity;
        ResourceBindingSlot slot                                         = ResourceBindingSlot::MESH;
        uint64_t            bindingRevision                              = 0;
        uint64_t            requestSequence                              = 0;
        bool                operator==(const ResourceSubscriber &) const = default;
    };
    struct ResourceVersionData {
        RenderResourceKey      resource;
        Name                   format;
        RenderResourceMetadata metadata;
    };
    struct ResolvedResourceBinding {
        ResourceSubscriber                         subscriber;
        RenderResourceKey                          desired;
        std::shared_ptr<const ResourceVersionData> selected;
    };

    // Pointer-free owned bytes use the provider's format schema; retain until CPU readers/copies finish.
    struct RenderResourcePayload {
        RenderResourceKey                 resource;
        Name                              format;
        std::vector<uint8_t>              bytes;
        RenderResourceMetadata            metadata;
        std::optional<RenderMeshData>     mesh;
        std::optional<RenderMaterialData> material;
    };

    inline uint64_t RenderResourceCpuBytes(const RenderResourcePayload &payload)
    {
        uint64_t   size = payload.bytes.size();
        const auto add  = [&](uint64_t count) { size = count > UINT64_MAX - size ? UINT64_MAX : size + count; };
        add(payload.metadata.lodDistances.size() * sizeof(float));
        if (payload.mesh) {
            add(payload.mesh->streams.size() * sizeof(RenderMeshStream));
            add(payload.mesh->lods.size() * sizeof(RenderMeshLod));
            add(payload.mesh->attributes.size() * sizeof(RasterVertexAttribute));
            for (const auto &attribute : payload.mesh->attributes)
                add(attribute.semantic.size());
        }
        if (payload.material) {
            const auto &material = *payload.material;
            add(material.vertexCode.size());
            add(material.fragmentCode.size());
            add(material.vertexEntry.size());
            add(material.fragmentEntry.size());
            add(material.reflection.resources.size() * sizeof(ShaderResource));
            add(material.reflection.blocks.size() * sizeof(ShaderBlockLayout));
            add(material.reflection.pushConstants.size() * sizeof(PushConstantRange));
            for (const auto &resource : material.reflection.resources)
                add(resource.name.size());
            for (const auto &block : material.reflection.blocks) {
                add(block.name.size());
                add(block.structName.size());
                add(block.members.size() * sizeof(ShaderBlockMember));
                for (const auto &member : block.members)
                    add(member.name.size());
            }
        }
        return size;
    }

    enum class RenderResourceState : uint8_t { CPU_READY, UPLOAD_QUEUED, UPLOAD_SUBMITTED, RESIDENT, FAILED, CANCELLED, REQUESTED };

    struct ResourceStateChanged {
        RenderResourceKey   resource;
        RenderResourceState state            = RenderResourceState::CPU_READY;
        uint64_t            submissionSerial = 0;
        RendererError       error; // meaningful for FAILED
    };

} // namespace sky::aurora
