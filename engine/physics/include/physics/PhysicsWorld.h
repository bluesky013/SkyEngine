//
// Created by blues on 2024/9/1.
//

#include <physics/RigidBody.h>
#include <physics/CharacterController.h>
#include <physics/PhysicsBase.h>
#include <physics/PhysicsQuery.h>
#include <physics/PhysicsDebugGeometry.h>
#include <framework/world/World.h>
#include <core/math/Transform.h>
#include <list>
#include <memory>
#include <vector>

namespace sky::phy {
    class PhysicsWorld : public IWorldSubSystem {
    public:
        PhysicsWorld() = default;
        ~PhysicsWorld() override = default;

        static constexpr std::string_view NAME = "PhysicsWorld";

        void AddRigidBody(RigidBody *rb);
        void RemoveRigidBody(RigidBody *rb);

        void AddCollisionObject(CollisionObject *obj);
        void RemoveCollisionObject(CollisionObject *obj);

        void AddCharacterController(CharacterController *rb);
        void RemoveCharacterController(CharacterController *rb);

        virtual void SetDebugDrawEnable(bool en) {}
        virtual void SetGravity(const Vector3 &gravity) {}

        // Backend-neutral queries. Defaults report no hit so a backend without support stays safe.
        virtual bool Raycast(const Vector3 &origin, const Vector3 &dir, float maxDistance, RaycastHit &out,
                             const CollisionFilters *filter = nullptr) const { return false; }

        virtual bool Sweep(const BoxShape &shape, const Transform &from, const Vector3 &dir, float maxDistance,
                           SweepResult &out, const CollisionFilters *filter = nullptr) const { return false; }

        virtual uint32_t Overlap(const BoxShape &shape, const Transform &pose, std::vector<OverlapResult> &out,
                                 const CollisionFilters *filter = nullptr) const { return 0; }

        // Fills render-agnostic debug geometry (no render resource types).
        virtual void CollectDebugGeometry(PhysicsDebugGeometry &out) const {}

    protected:
        virtual void AddRigidBodyImpl(RigidBody *rb) = 0;
        virtual void RemoveRigidBodyImpl(RigidBody *rb) = 0;
        virtual void AddCollisionObjectImpl(CollisionObject *obj) = 0;
        virtual void RemoveCollisionObjectImpl(CollisionObject *obj) = 0;
        virtual void AddCharacterControllerImpl(CharacterController *rb) = 0;
        virtual void RemoveCharacterControllerImpl(CharacterController *rb) = 0;

        std::list<std::unique_ptr<RigidBody>>           rigidBodies;
        std::list<std::unique_ptr<CollisionObject>>     collisionObjects;
        std::list<std::unique_ptr<CharacterController>> characterControllers;
    };

} // namespace sky::phy