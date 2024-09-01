//
// Created by blues on 2024/9/1.
//

#pragma once

namespace sky::phy {

    class CharacterController {
    public:
        CharacterController() = default;
        virtual ~CharacterController() = default;
    };

    class CharacterControllerFactory {
    public:
        CharacterControllerFactory() = default;
        ~CharacterControllerFactory() = default;

//        CharacterController* Create
    };

} // namespace sky::phy
