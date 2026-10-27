//
// Created on 2026/10/26.
//

#include "JoltBackend.h"
#include "JoltWorld.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <core/environment/Singleton.h>

namespace sky::phy {

    struct JoltSdk {
        std::shared_ptr<const void> codeLease;
        ~JoltSdk()
        {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    };

    class JoltSdkState final : public Singleton<JoltSdkState> {
    public:
        std::mutex               debugMutex;
        std::shared_ptr<JoltSdk> Acquire(const std::shared_ptr<const void> &lease)
        {
            std::lock_guard lock(mutex);
            if (auto existing = sdk.lock()) {
                return existing;
            }
            if (JPH::Factory::sInstance != nullptr) {
                return {};
            }
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
            auto created       = std::shared_ptr<JoltSdk>(new JoltSdk(), [this](JoltSdk *state) {
                std::lock_guard releaseLock(mutex);
                delete state;
            });
            created->codeLease = lease;
            codeLease          = lease;
            sdk                = created;
            return created;
        }

    private:
        friend class Singleton<JoltSdkState>;
        JoltSdkState() = default;
        std::shared_ptr<const void> codeLease;
        std::mutex                  mutex;
        std::weak_ptr<JoltSdk>      sdk;
    };

    std::mutex &JoltDebugMutex()
    {
        return JoltSdkState::Get()->debugMutex;
    }

    JoltExecutor::JoltExecutor(uint32_t workers) : jobs(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, static_cast<int>(workers))
    {
    }

    std::shared_ptr<JoltExecutor> JoltSession::GetExecutor(uint32_t workers)
    {
        std::lock_guard lock(executorMutex);
        auto            result = executors[workers].lock();
        if (!result) {
            result             = std::make_shared<JoltExecutor>(workers);
            executors[workers] = result;
        }
        return result;
    }

    JoltBackend::JoltBackend(std::shared_ptr<const void> lease) : codeLease(std::move(lease))
    {
        caps.jobStepping         = true;
        caps.supportsCCD         = true;
        caps.supportsConstraints = true;
        caps.supportsCharacters  = true;
    }

    JoltBackend::~JoltBackend() = default;

    bool JoltBackend::Init()
    {
        if (session) {
            return true;
        }
        auto sdk = JoltSdkState::Get()->Acquire(codeLease);
        if (!sdk) {
            status = {PhysicsError::InvalidState, "Jolt Factory has an unknown owner"};
            return false;
        }
        session            = std::make_shared<JoltSession>();
        session->codeLease = codeLease;
        session->sdk       = std::move(sdk);
        status             = {};
        return true;
    }

    void JoltBackend::Shutdown()
    {
        session.reset();
    }
    const PhysicsBackendCaps &JoltBackend::GetCaps() const
    {
        return caps;
    }
    PhysicsStatus JoltBackend::GetLastStatus() const
    {
        return status;
    }

    IPhysicsWorld *JoltBackend::CreateWorld(const PhysicsWorldDesc &desc)
    {
        if (!session) {
            status = {PhysicsError::InvalidState, "Initialize the backend before creating a world"};
            return nullptr;
        }
        auto world = std::make_unique<JoltWorld>(session);
        status     = world->Init(desc);
        return status ? world.release() : nullptr;
    }

    void JoltBackend::DestroyWorld(IPhysicsWorld *world)
    {
        delete world;
    }

} // namespace sky::phy
