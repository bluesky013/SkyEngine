#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <particle/assets/ParticleEffectAsset.h>

#include <memory>
#include <unordered_map>

namespace sky::particle {

    // Editor-only in-memory effects. The world subsystem consults this before the product
    // cache, so an unsaved draft previews through the same simulation/render path.
    class ParticlePreviewEffectRegistry : public Singleton<ParticlePreviewEffectRegistry> {
        friend class Singleton<ParticlePreviewEffectRegistry>;

    public:
        void Set(const Uuid &world, const Uuid &effect, std::shared_ptr<const ParticleEffectAssetData> data)
        {
            byWorld[world][effect] = std::move(data);
            ++revision;
        }

        void Clear(const Uuid &world, const Uuid &effect)
        {
            const auto found = byWorld.find(world);
            if (found == byWorld.end() || found->second.erase(effect) == 0)
                return;
            if (found->second.empty())
                byWorld.erase(found);
            ++revision;
        }

        void ClearAll()
        {
            if (byWorld.empty())
                return;
            byWorld.clear();
            ++revision;
        }

        std::shared_ptr<const ParticleEffectAssetData> Find(const Uuid &world, const Uuid &effect) const
        {
            const auto found = byWorld.find(world);
            if (found == byWorld.end())
                return nullptr;
            const auto effectFound = found->second.find(effect);
            return effectFound == found->second.end() ? nullptr : effectFound->second;
        }

        uint64_t GetRevision() const
        {
            return revision;
        }

    private:
        ParticlePreviewEffectRegistry() = default;

        std::unordered_map<Uuid, std::unordered_map<Uuid, std::shared_ptr<const ParticleEffectAssetData>>> byWorld;
        uint64_t                                                                                          revision = 0;
    };

} // namespace sky::particle
