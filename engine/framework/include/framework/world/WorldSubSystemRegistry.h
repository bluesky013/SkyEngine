//
// Created on 2026/10/07.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/world/World.h>
#include <framework/world/WorldDesc.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky {

    // Creates a subsystem for a world from an optional reflected config.
    using WorldSubSystemFactory = std::function<std::unique_ptr<IWorldSubSystem>(World &world, const Any &config)>;

    // Everything a plugin declares for a world subsystem: how to create it, the
    // reflected config type (nullable), a default config for the editor, and an
    // optional validator that reports a reason on failure.
    struct WorldSubSystemRegistration {
        WorldSubSystemFactory                                      factory;
        const TypeInfoRT                                          *configType = nullptr;
        std::function<Any()>                                       makeDefaultConfig;
        std::function<bool(const Any &config, std::string &error)> validate;
    };

    // Process-wide registry of world subsystem registrations. Derives from
    // Singleton so a single instance is shared across module DLLs (via Environment),
    // mirroring PhysicsBackendRegistry's "register once, attach per world" pattern.
    class WorldSubSystemRegistry : public Singleton<WorldSubSystemRegistry> {
    public:
        static WorldSubSystemRegistry &Get()
        {
            return *Singleton<WorldSubSystemRegistry>::Get();
        }

        // Registers (or overrides, logged) the registration for a name.
        bool Register(const Name &name, WorldSubSystemRegistration registration);
        void Unregister(const Name &name);
        bool IsRegistered(const Name &name) const;

        const WorldSubSystemRegistration *GetRegistration(const Name &name) const;

        // Returns nullptr when the name is not registered.
        std::unique_ptr<IWorldSubSystem> Create(const Name &name, World &world, const Any &config) const;

        std::vector<Name> GetNames() const;

        // Test-only: drops all registrations.
        void Clear();

    private:
        friend class Singleton<WorldSubSystemRegistry>;
        WorldSubSystemRegistry() = default;

        std::unordered_map<Name, WorldSubSystemRegistration> registrations;
    };

} // namespace sky
