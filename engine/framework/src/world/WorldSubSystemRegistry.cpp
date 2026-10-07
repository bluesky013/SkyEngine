//
// Created on 2026/10/07.
//

#include <framework/world/WorldSubSystemRegistry.h>

#include <core/logger/Logger.h>

#include <algorithm>

static const char *TAG = "WorldSubSystemRegistry";

namespace sky {

    bool WorldSubSystemRegistry::Register(const Name &name, WorldSubSystemRegistration registration)
    {
        if (registration.factory == nullptr) {
            return false;
        }
        const auto it = registrations.find(name);
        if (it != registrations.end()) {
            LOG_W(TAG, "world subsystem '%.*s' re-registered, overriding", static_cast<int>(name.GetStr().size()), name.GetStr().data());
            it->second = std::move(registration);
            return true;
        }
        registrations.emplace(name, std::move(registration));
        return true;
    }

    void WorldSubSystemRegistry::Unregister(const Name &name)
    {
        registrations.erase(name);
    }

    bool WorldSubSystemRegistry::IsRegistered(const Name &name) const
    {
        return registrations.find(name) != registrations.end();
    }

    const WorldSubSystemRegistration *WorldSubSystemRegistry::GetRegistration(const Name &name) const
    {
        const auto it = registrations.find(name);
        return it != registrations.end() ? &it->second : nullptr;
    }

    std::unique_ptr<IWorldSubSystem> WorldSubSystemRegistry::Create(const Name &name, World &world, const Any &config) const
    {
        const auto it = registrations.find(name);
        if (it == registrations.end()) {
            return nullptr;
        }
        return it->second.factory(world, config);
    }

    std::vector<Name> WorldSubSystemRegistry::GetNames() const
    {
        std::vector<Name> names;
        names.reserve(registrations.size());
        for (const auto &[name, registration] : registrations) {
            names.push_back(name);
        }
        // Deterministic order for UI listings (registrations live in a hash map).
        std::sort(names.begin(), names.end(), [](const Name &a, const Name &b) { return a.GetStr() < b.GetStr(); });
        return names;
    }

    void WorldSubSystemRegistry::Clear()
    {
        registrations.clear();
    }

} // namespace sky
