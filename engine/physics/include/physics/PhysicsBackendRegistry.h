//
// Created on 2026/09/23.
//

#pragma once

#include <physics/IPhysicsBackend.h>

#include <framework/world/World.h>

#include <functional>
#include <memory>

namespace sky::phy {

    // Factory registered by the backend plugin that creates its world sub-system (under
    // PHYSICS_SYSTEM_NAME) for a newly created world, so hosts (launcher/editor) attach physics without
    // depending on a specific backend plugin.
    using PhysicsWorldAttacher = std::function<IWorldSubSystem *(World &)>;

    // Engine-side factory registry for the active physics backend. Exactly one backend is active at a
    // time; registering a new one shuts the previous one down and releases it.
    class PhysicsBackendRegistry {
    public:
        static PhysicsBackendRegistry &Get();

        // Takes ownership of the backend. Shuts down and releases any previously registered backend.
        bool Register(std::unique_ptr<IPhysicsBackend> backend);
        void Unregister();

        bool HasBackend() const { return backend != nullptr; }
        const PhysicsBackendCaps *GetCaps() const;

        // True when the active backend advertises support for the requested math mode.
        bool IsModeAvailable(PhysicsMathMode mode) const;

        // Returns nullptr when no backend is registered or the mode is unavailable.
        IPhysicsWorld *CreateWorld(const PhysicsWorldDesc &desc);
        void DestroyWorld(IPhysicsWorld *world);

        // Host attachment: the backend plugin registers an attacher that adds its sub-system to a world.
        void             SetWorldAttacher(PhysicsWorldAttacher attacher);
        IWorldSubSystem *AttachToWorld(World &world);

    private:
        PhysicsBackendRegistry() = default;

        std::unique_ptr<IPhysicsBackend> backend;
        PhysicsWorldAttacher             worldAttacher;
    };

} // namespace sky::phy
