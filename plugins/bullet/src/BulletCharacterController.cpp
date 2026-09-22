//
// Created by blues on 2024/9/2.
//

#include <bullet/BulletCharacterController.h>
#include <bullet/BulletPhysicsWorld.h>
#include <bullet/BulletConversion.h>

#include <algorithm>

namespace sky::phy {

    BulletCharacterController::~BulletCharacterController()
    {
        Destroy();
    }

    void BulletCharacterController::Destroy()
    {
        if (world != nullptr) {
            if (controller != nullptr) {
                world->GetWorld()->removeAction(controller.get());
            }
            if (ghost != nullptr) {
                world->GetWorld()->removeCollisionObject(ghost.get());
            }
        }
        controller.reset();
        ghost.reset();
        shape.reset();
    }

    void BulletCharacterController::SetPhysicsWorld(BulletPhysicsWorld *wd)
    {
        Destroy();
        world = wd;
        Build();
    }

    void BulletCharacterController::Build()
    {
        if (world == nullptr || controller != nullptr) {
            return;
        }

        const float cylinderHeight = std::max(height - 2.f * radius, 0.01f);
        shape = std::make_unique<btCapsuleShape>(radius, cylinderHeight);

        ghost = std::make_unique<btPairCachingGhostObject>();
        ghost->setCollisionShape(shape.get());
        ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
        btTransform start;
        start.setIdentity();
        ghost->setWorldTransform(start);

        controller = std::make_unique<btKinematicCharacterController>(ghost.get(), shape.get(), stepHeight);
        controller->setGravity(ToBullet(gravity));
        controller->setMaxSlope(slopeLimitDeg * 0.017453292f);

        world->GetWorld()->addCollisionObject(ghost.get(), btBroadphaseProxy::CharacterFilter,
                                              btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::CharacterFilter);
        world->GetWorld()->addAction(controller.get());

        if (hasInitialPosition) {
            controller->warp(ToBullet(initialTranslation));
        }
    }

    void BulletCharacterController::SetWorldTransform(const Transform &trans)
    {
        if (controller != nullptr) {
            controller->warp(ToBullet(trans.translation));
        } else {
            initialTranslation  = trans.translation;
            hasInitialPosition  = true;
        }
    }

    Transform BulletCharacterController::GetWorldTransform() const
    {
        if (ghost != nullptr) {
            return FromBullet(ghost->getWorldTransform());
        }
        return Transform::GetIdentity();
    }

    void BulletCharacterController::SetCapsule(float inRadius, float inHeight)
    {
        radius = inRadius;
        height = inHeight;

        if (controller != nullptr) {
            auto *previousWorld = world;
            Destroy();
            world = previousWorld;
            Build();
        }
    }

    void BulletCharacterController::SetGravity(const Vector3 &inGravity)
    {
        gravity = inGravity;
        if (controller != nullptr) {
            controller->setGravity(ToBullet(gravity));
        }
    }

    void BulletCharacterController::SetStepHeight(float inStepHeight)
    {
        stepHeight = inStepHeight;
        if (controller != nullptr) {
            controller->setStepHeight(stepHeight);
        }
    }

    void BulletCharacterController::SetSlopeLimit(float degrees)
    {
        slopeLimitDeg = degrees;
        if (controller != nullptr) {
            controller->setMaxSlope(slopeLimitDeg * 0.017453292f);
        }
    }

    bool BulletCharacterController::Move(const Vector3 &displacement)
    {
        if (controller == nullptr) {
            return false;
        }
        controller->setWalkDirection(ToBullet(displacement));
        return true;
    }

    bool BulletCharacterController::IsGrounded() const
    {
        return controller != nullptr && controller->onGround();
    }

} // namespace sky::phy
