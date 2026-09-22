//
// Created on 2026/09/22.
//

#include <bullet/BulletRegistry.h>

#include <physics/PhysicsRegistry.h>
#include <physics/PhysicsWorld.h>
#include <physics/CollisionObject.h>
#include <physics/CharacterController.h>
#include <physics/PhysicsShape.h>
#include <physics/PhysicsBase.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::phy;

namespace {

    Transform MakeTransform(const Vector3 &translation)
    {
        Transform transform;
        transform.translation = translation;
        return transform;
    }

} // namespace

TEST(BulletCharacterControllerTest, FallsAndGetsGrounded)
{
    RegisterBulletPhysics();

    PhysicsWorld *world = PhysicsRegistry::Get()->CreatePhysicsWorld();
    ASSERT_NE(world, nullptr);

    // Ground box: top surface at y = 0.
    BoxShape groundBox;
    groundBox.halfExt = Vector3(100.f, 1.f, 100.f);

    CollisionObject *ground = PhysicsRegistry::Get()->CreateCollisionObject();
    ground->SetShape(new PhysicsBoxShape(groundBox));
    ground->SetWorldTransform(MakeTransform(Vector3(0.f, -1.f, 0.f)));
    world->AddCollisionObject(ground);

    // Character capsule starting above the ground.
    CharacterController *controller = PhysicsRegistry::Get()->CreateCharacterController();
    controller->SetCapsule(0.5f, 2.f);
    controller->SetWorldTransform(MakeTransform(Vector3(0.f, 5.f, 0.f)));
    world->AddCharacterController(controller);

    const float startY = controller->GetWorldTransform().translation.y;
    EXPECT_FLOAT_EQ(startY, 5.f);

    for (int i = 0; i < 240; ++i) {
        world->Tick(1.f / 60.f);
    }

    const float endY = controller->GetWorldTransform().translation.y;
    EXPECT_LT(endY, startY);                 // fell under gravity
    EXPECT_LT(endY, 2.f);                    // reached near the ground
    EXPECT_TRUE(controller->IsGrounded());

    world->RemoveCharacterController(controller);
    world->RemoveCollisionObject(ground);
    UnregisterBulletPhysics();
}
