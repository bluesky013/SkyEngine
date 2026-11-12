#pragma once

#include <memory>
#include <physics/softbody/SoftBodyTypes.h>
#include <physics/softbody/SoftBodyWorldTypes.h>

namespace sky::phy {
    class IPhysicsSystem;
    class IPhysicsWorld;
} // namespace sky::phy

namespace sky::softbody {
    class CpuSoftBodySolver;

    class SoftBodyWorldMember {
    public:
        virtual ~SoftBodyWorldMember()                                                              = default;
        virtual Uuid               GetOrderKey() const                                              = 0;
        virtual CpuSoftBodySolver *GetSolver() const                                                = 0;
        virtual phy::PhysicsStatus ReadDebugBody(phy::SoftBodyDebugBody &out) const                 = 0;
        virtual phy::PhysicsStatus UpdateRoot(phy::IPhysicsWorld &physics)                          = 0;
        virtual phy::PhysicsStatus ValidatePublication(const phy::SoftBodySnapshot &snapshot) const = 0;
        virtual void               Publish(phy::SoftBodySnapshot snapshot)                          = 0;
        virtual void               Report(const phy::PhysicsStatus &status)                         = 0;
        virtual void               Close(phy::IPhysicsWorld &physics)                               = 0;
    };

    class SoftBodyWorldRegistration {
    public:
        virtual ~SoftBodyWorldRegistration()                   = default;
        virtual void Remove(const SoftBodyWorldMember *member) = 0;
    };

    phy::PhysicsStatus RegisterSoftBodyWorldMember(const Uuid                                 &world,
                                                   phy::IPhysicsSystem                        &physics,
                                                   const std::shared_ptr<SoftBodyWorldMember> &member,
                                                   std::shared_ptr<SoftBodyWorldRegistration> &out);
} // namespace sky::softbody
