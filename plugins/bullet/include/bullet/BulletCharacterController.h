//
// Created by blues on 2024/9/2.
//

#pragma once

#include <physics/CharacterController.h>
#include <btBulletCollisionCommon.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <memory>

namespace sky::phy {
    class BulletPhysicsWorld;

    class BulletCharacterController : public CharacterController {
    public:
        BulletCharacterController() = default;
        ~BulletCharacterController() override;

        void SetPhysicsWorld(BulletPhysicsWorld *wd);

        void      SetWorldTransform(const Transform &trans) override;
        Transform GetWorldTransform() const override;
        void      SetCapsule(float radius, float height) override;
        void      SetGravity(const Vector3 &gravity) override;
        void      SetStepHeight(float height) override;
        void      SetSlopeLimit(float degrees) override;
        bool      Move(const Vector3 &displacement) override;
        bool      IsGrounded() const override;

    private:
        void Build();
        void Destroy();

        BulletPhysicsWorld *world = nullptr;
        float   radius        = 0.5f;
        float   height        = 2.f;
        float   stepHeight    = 0.3f;
        float   slopeLimitDeg = 45.f;
        Vector3 gravity       = Vector3(0.f, -9.8f, 0.f);
        Vector3 initialTranslation;
        bool    hasInitialPosition = false;

        std::unique_ptr<btPairCachingGhostObject>       ghost;
        std::unique_ptr<btCapsuleShape>                 shape;
        std::unique_ptr<btKinematicCharacterController> controller;
    };

} // namespace sky::phy
