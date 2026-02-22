//
// Created by blues on 2024/9/2.
//

#pragma once

#include <physics/CharacterController.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <memory>

namespace sky::phy {

    class BulletCharacterController : public CharacterController {
    public:
        BulletCharacterController() = default;
        ~BulletCharacterController() override = default;

    private:
        std::unique_ptr<btKinematicCharacterController> controller;
    };

} // namespace sky::phy
