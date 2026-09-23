//
// Created on 2026/09/23.
//

#include <physics/PhysicsBackendRegistry.h>

#include <utility>

namespace sky::phy {

    PhysicsBackendRegistry &PhysicsBackendRegistry::Get()
    {
        static PhysicsBackendRegistry instance;
        return instance;
    }

    bool PhysicsBackendRegistry::Register(std::unique_ptr<IPhysicsBackend> newBackend)
    {
        if (!newBackend) {
            return false;
        }

        Unregister();

        backend = std::move(newBackend);
        if (!backend->Init()) {
            backend.reset();
            return false;
        }
        return true;
    }

    void PhysicsBackendRegistry::Unregister()
    {
        if (backend) {
            backend->Shutdown();
            backend.reset();
        }
    }

    const PhysicsBackendCaps *PhysicsBackendRegistry::GetCaps() const
    {
        return backend ? &backend->GetCaps() : nullptr;
    }

    bool PhysicsBackendRegistry::IsModeAvailable(PhysicsMathMode mode) const
    {
        if (!backend) {
            return false;
        }
        const auto &caps = backend->GetCaps();
        if (mode == PhysicsMathMode::Exact) {
            return caps.deterministic && caps.mathMode == PhysicsMathMode::Exact;
        }
        return true;
    }

    IPhysicsWorld *PhysicsBackendRegistry::CreateWorld(const PhysicsWorldDesc &desc)
    {
        if (!backend || !IsModeAvailable(desc.mathMode)) {
            return nullptr;
        }
        return backend->CreateWorld(desc);
    }

    void PhysicsBackendRegistry::DestroyWorld(IPhysicsWorld *world)
    {
        if (backend && world != nullptr) {
            backend->DestroyWorld(world);
        }
    }

    void PhysicsBackendRegistry::SetWorldAttacher(PhysicsWorldAttacher attacher)
    {
        worldAttacher = std::move(attacher);
    }

    IWorldSubSystem *PhysicsBackendRegistry::AttachToWorld(World &world)
    {
        return worldAttacher ? worldAttacher(world) : nullptr;
    }

} // namespace sky::phy
