//
// Created on 2026/09/23.
//

#pragma once

#include <physics/IPhysicsBackend.h>

#include <core/environment/Singleton.h>

#include <memory>

namespace sky::phy {

    // Engine-side factory registry for the active physics backend. Exactly one backend is active at a
    // time; registering a new one shuts the previous one down and releases it.
    //
    // Process-wide (cross-DLL): engine/physics is a static lib linked into the backend plugin and the
    // hosts, so the instance lives in the shared Environment (Singleton), not a function-local static.
    class PhysicsBackendRegistry : public Singleton<PhysicsBackendRegistry> {
    public:
        static PhysicsBackendRegistry &Get();

        // Takes ownership of the backend. Shuts down and releases any previously registered backend.
        bool Register(std::unique_ptr<IPhysicsBackend> backend);
        void Unregister();

        bool HasBackend() const
        {
            return backend != nullptr;
        }
        const PhysicsBackendCaps *GetCaps() const;

        // True when the active backend advertises support for the requested math mode.
        bool IsModeAvailable(PhysicsMathMode mode) const;

        // Returns nullptr when no backend is registered or the mode is unavailable.
        IPhysicsWorld *CreateWorld(const PhysicsWorldDesc &desc);
        void           DestroyWorld(IPhysicsWorld *world);

    private:
        friend class Singleton<PhysicsBackendRegistry>;

        PhysicsBackendRegistry() = default;

        std::unique_ptr<IPhysicsBackend> backend;
    };

} // namespace sky::phy
