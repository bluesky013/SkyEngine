//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

namespace sky {

    class GameObject;

    class Component {
    public:
        Component() = default;
        virtual ~Component() = default;

    protected:
        friend class GameObject;
        GameObject* object = nullptr;
    };

}