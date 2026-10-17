//
// Created on 2026/10/17.
//

#include "graph/FrameFeatureRegistry.h"
#include <algorithm>
#include <set>

namespace sky::aurora {
    std::optional<RendererError> FrameFeatureRegistry::Validate(const FramePacket &packet) const
    {
        if (packet.featureCommands.size() > 64 || packet.extensions.size() > 64)
            return RendererError{RendererErrorCode::INVALID_PACKET, "Feature packet budget exceeded"};
        auto               projected = entries;
        std::set<uint32_t> changed;
        for (const auto &command : packet.featureCommands) {
            const auto id = std::visit([](const auto &value) { return value.id; }, command);
            if (!id.IsValid() || !changed.insert(id.index).second)
                return RendererError{RendererErrorCode::INVALID_PACKET, "Feature requires a unique valid command per packet"};
            auto found = projected.find(id.index);
            if (const auto *add = std::get_if<RegisterRenderFeature>(&command)) {
                if (!add->feature || !add->moduleLease || add->name.Empty() || add->feature->GetName() != add->name ||
                    (found != projected.end() && (found->second.lease || id.generation <= found->second.generation)) ||
                    std::any_of(projected.begin(), projected.end(), [&](const auto &entry) {
                        return entry.second.lease &&
                               (entry.second.lease->registration.name == add->name || entry.second.lease->registration.feature == add->feature);
                    }))
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Feature registration conflicts with an active or retiring generation"};
                if (found == projected.end() && projected.size() >= 1024)
                    return RendererError{RendererErrorCode::OUT_OF_MEMORY, "Feature handle budget exceeded"};
                projected[id.index] = {id.generation, std::make_shared<FrameFeatureLease>(FrameFeatureLease{*add}), false};
            } else {
                if (found == projected.end() || found->second.generation != id.generation || !found->second.lease || found->second.retiring)
                    return RendererError{RendererErrorCode::STALE_HANDLE, "Feature generation is unavailable"};
                if (const auto *enable = std::get_if<SetRenderFeatureEnabled>(&command);
                    enable && enable->enabled != found->second.lease->enabled && found->second.lease->activation == UINT64_MAX)
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Feature activation counter exhausted; retire this generation"};
                if (std::holds_alternative<RetireRenderFeature>(command))
                    found->second.retiring = true;
            }
        }
        uint64_t bytes = 0;
        for (const auto &payload : packet.extensions) {
            bytes += payload.bytes.size();
            if (bytes > 16 * 1024 * 1024 || payload.outputs.size() > 64 || payload.resources.size() > 1024 ||
                std::any_of(payload.outputs.begin(), payload.outputs.end(), [](auto id) { return !id.IsValid(); }))
                return RendererError{RendererErrorCode::INVALID_PACKET, "Invalid feature payload budget or output"};
            const auto found = std::find_if(projected.begin(), projected.end(), [&](const auto &entry) {
                return entry.second.lease && !entry.second.retiring && entry.second.lease->registration.name == payload.feature;
            });
            if (found == projected.end())
                return RendererError{RendererErrorCode::INVALID_PACKET, "Extension requires a registered feature"};
            if (auto error = found->second.lease->registration.feature->Validate(payload))
                return error;
        }
        return {};
    }
    void FrameFeatureRegistry::Apply(FramePacket &packet)
    {
        for (auto &command : packet.featureCommands) {
            const auto id    = std::visit([](const auto &value) { return value.id; }, command);
            auto      &entry = entries[id.index];
            if (auto *add = std::get_if<RegisterRenderFeature>(&command))
                entry = {id.generation, std::make_shared<FrameFeatureLease>(FrameFeatureLease{std::move(*add)}), false};
            else if (const auto *enable = std::get_if<SetRenderFeatureEnabled>(&command)) {
                if (entry.lease->enabled != enable->enabled)
                    ++entry.lease->activation;
                entry.lease->enabled = enable->enabled;
            } else {
                entry.lease->enabled = false;
                entry.retiring       = true;
            }
        }
        packet.featureCommands.clear();
    }
    std::vector<FrameFeatureUse> FrameFeatureRegistry::Snapshot() const
    {
        std::vector<FrameFeatureUse> result;
        for (const auto &[index, entry] : entries)
            if (entry.lease && entry.lease->enabled)
                result.push_back({entry.lease, entry.lease->activation});
        return result;
    }
    std::vector<RenderFeatureId> FrameFeatureRegistry::CollectRetired()
    {
        std::vector<RenderFeatureId> result;
        for (auto &[index, entry] : entries)
            if (entry.retiring && entry.lease.use_count() == 1) {
                entry.lease->registration.feature->OnRetired();
                entry.lease.reset();
                entry.retiring = false;
                result.push_back({index, entry.generation});
            }
        return result;
    }
    void FrameFeatureRegistry::Shutdown()
    {
        for (auto &[index, entry] : entries)
            if (entry.lease) {
                entry.lease->enabled = false;
                entry.retiring       = true;
            }
    }
} // namespace sky::aurora
