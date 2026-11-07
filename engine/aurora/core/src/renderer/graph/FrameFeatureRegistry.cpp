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
        auto               projected           = entries;
        auto               projectedExtensions = extensions;
        std::set<uint32_t> changed;
        for (const auto &command : packet.featureCommands) {
            const auto id = std::visit([](const auto &value) { return value.id; }, command);
            if (!id.IsValid() || !changed.insert(id.index).second)
                return RendererError{RendererErrorCode::INVALID_PACKET, "Feature requires a unique valid command per packet"};
            auto found = projected.find(id.index);
            if (const auto *add = std::get_if<RegisterSceneExtension>(&command)) {
                if (!add->extension || add->extension->GetName().Empty() || !add->scene.IsValid())
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Scene extension requires an identity and active scene"};
                const auto scene = projected.find(add->scene.index);
                if (scene == projected.end() || scene->second.generation != add->scene.generation || !scene->second.lease || scene->second.retiring ||
                    !scene->second.lease->registration.feature->SupportsSceneExtensions())
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Scene extension target is unavailable"};
                const auto existing = projectedExtensions.find(id.index);
                if (found != projected.end() ||
                    (existing != projectedExtensions.end() && (existing->second.lease || id.generation <= existing->second.generation)) ||
                    std::any_of(projectedExtensions.begin(), projectedExtensions.end(),
                                [&](const auto &entry) {
                                    return entry.second.lease && (entry.second.lease->registration.extension == add->extension ||
                                                                  entry.second.lease->registration.extension->GetName() == add->extension->GetName());
                                }) ||
                    std::any_of(projected.begin(), projected.end(), [&](const auto &entry) {
                        return entry.second.lease && entry.second.lease->registration.name == add->extension->GetName();
                    }))
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Scene extension identity conflicts with an existing generation"};
                const auto count = std::count_if(projectedExtensions.begin(), projectedExtensions.end(), [&](const auto &entry) {
                    return entry.second.lease && entry.second.lease->registration.scene == add->scene;
                });
                if (count >= 64 || (existing == projectedExtensions.end() && projectedExtensions.size() >= 1024))
                    return RendererError{RendererErrorCode::OUT_OF_MEMORY, "Scene extension capacity exceeded"};
                projectedExtensions[id.index] = {id.generation, std::make_shared<SceneExtensionLease>(SceneExtensionLease{*add, scene->second.lease}),
                                                 false};
            } else if (const auto *retire = std::get_if<RetireSceneExtension>(&command)) {
                const auto entry = projectedExtensions.find(id.index);
                if (entry == projectedExtensions.end() || entry->second.generation != id.generation || !entry->second.lease ||
                    entry->second.retiring || entry->second.lease->registration.scene != retire->scene)
                    return RendererError{RendererErrorCode::STALE_HANDLE, "Scene extension generation is unavailable"};
                entry->second.retiring = true;
            } else if (const auto *add = std::get_if<RegisterRenderFeature>(&command)) {
                if (projectedExtensions.contains(id.index) ||
                    std::any_of(projectedExtensions.begin(), projectedExtensions.end(),
                                [&](const auto &entry) {
                                    return entry.second.lease && entry.second.lease->registration.extension->GetName() == add->name;
                                }) ||
                    !add->feature || add->name.Empty() || add->feature->GetName() != add->name ||
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
                if (std::holds_alternative<RetireRenderFeature>(command)) {
                    found->second.retiring = true;
                    for (auto &[index, extension] : projectedExtensions)
                        if (extension.lease && extension.lease->registration.scene == id)
                            extension.retiring = true;
                }
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
            if (found != projected.end()) {
                if (auto error = found->second.lease->registration.feature->Validate(payload))
                    return error;
                continue;
            }
            const auto extension = std::find_if(projectedExtensions.begin(), projectedExtensions.end(), [&](const auto &entry) {
                return entry.second.lease && !entry.second.retiring && entry.second.lease->registration.extension->GetName() == payload.feature;
            });
            if (extension == projectedExtensions.end())
                return RendererError{RendererErrorCode::INVALID_PACKET, "Extension requires an active registration"};
            if (auto error = extension->second.lease->registration.extension->Validate(payload))
                return error;
        }
        return {};
    }
    void FrameFeatureRegistry::Apply(FramePacket &packet)
    {
        if (packet.featureCommands.empty())
            return;
        for (auto &command : packet.featureCommands) {
            const auto id = std::visit([](const auto &value) { return value.id; }, command);
            if (auto *add = std::get_if<RegisterRenderFeature>(&command)) {
                entries[id.index] = {id.generation, std::make_shared<FrameFeatureLease>(FrameFeatureLease{std::move(*add)}), false};
                continue;
            }
            if (auto *add = std::get_if<RegisterSceneExtension>(&command)) {
                auto scene           = entries.at(add->scene.index).lease;
                extensions[id.index] = {id.generation, std::make_shared<SceneExtensionLease>(SceneExtensionLease{std::move(*add), std::move(scene)}),
                                        false};
                continue;
            }
            if (const auto *retire = std::get_if<RetireSceneExtension>(&command)) {
                auto &entry          = extensions.at(retire->id.index);
                entry.lease->enabled = false;
                entry.retiring       = true;
                continue;
            }
            const auto found = entries.find(id.index);
            if (found == entries.end() || !found->second.lease)
                continue;
            auto &entry = found->second;
            if (const auto *enable = std::get_if<SetRenderFeatureEnabled>(&command)) {
                if (entry.lease->enabled != enable->enabled)
                    ++entry.lease->activation;
                entry.lease->enabled = enable->enabled;
            } else if (std::holds_alternative<RetireRenderFeature>(command)) {
                entry.lease->enabled = false;
                entry.retiring       = true;
                for (auto &[index, extension] : extensions)
                    if (extension.lease && extension.lease->registration.scene == id) {
                        extension.lease->enabled = false;
                        extension.retiring       = true;
                    }
            }
        }
        extensionOrder.clear();
        for (const auto &[index, entry] : extensions)
            if (entry.lease && !entry.retiring)
                extensionOrder.push_back(index);
        std::sort(extensionOrder.begin(), extensionOrder.end(), [&](uint32_t a, uint32_t b) {
            const auto &left  = extensions.at(a).lease->registration.extension;
            const auto &right = extensions.at(b).lease->registration.extension;
            if (left->GetOrder() != right->GetOrder())
                return left->GetOrder() < right->GetOrder();
            return std::string_view(left->GetName().GetStr()) < std::string_view(right->GetName().GetStr());
        });
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
    std::vector<SceneExtensionUse> FrameFeatureRegistry::SnapshotExtensions() const
    {
        std::vector<SceneExtensionUse> result;
        for (const auto index : extensionOrder) {
            const auto &entry = extensions.at(index);
            if (entry.lease && entry.lease->enabled && entry.lease->scene->enabled)
                result.push_back({entry.lease, entry.lease->scene->activation});
        }
        return result;
    }
    std::vector<RenderFeatureId> FrameFeatureRegistry::CollectRetired()
    {
        std::vector<RenderFeatureId> result;
        for (auto &[index, entry] : extensions)
            if (entry.retiring && entry.lease.use_count() == 1) {
                entry.lease->registration.extension->OnRetired();
                entry.lease.reset();
                entry.retiring = false;
                result.push_back({index, entry.generation});
            }
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
        for (auto &[index, entry] : extensions)
            if (entry.lease) {
                entry.lease->enabled = false;
                entry.retiring       = true;
            }
        for (auto &[index, entry] : entries)
            if (entry.lease) {
                entry.lease->enabled = false;
                entry.retiring       = true;
            }
    }
} // namespace sky::aurora
