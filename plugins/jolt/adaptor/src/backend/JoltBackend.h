//
// Created on 2026/10/26.
//

#pragma once

#include <physics/IPhysicsBackend.h>

#include <memory>

namespace sky::phy {

    struct JoltSession;

    class JoltBackend final : public IPhysicsBackend {
    public:
        explicit JoltBackend(std::shared_ptr<const void> codeLease = {});
        ~JoltBackend() override;
        bool                      Init() override;
        void                      Shutdown() override;
        const PhysicsBackendCaps &GetCaps() const override;
        PhysicsStatus             GetLastStatus() const override;
        IPhysicsWorld            *CreateWorld(const PhysicsWorldDesc &desc) override;
        void                      DestroyWorld(IPhysicsWorld *world) override;

    private:
        std::shared_ptr<const void>  codeLease;
        std::shared_ptr<JoltSession> session;
        PhysicsBackendCaps           caps;
        PhysicsStatus                status;
    };

} // namespace sky::phy
