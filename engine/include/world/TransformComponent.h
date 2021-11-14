//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <core/math/Transform.h>
#include <world/Component.h>
#include <vector>

namespace sky {

    class TransformComponent : public Component {
    public:
        TransformComponent() = default;
        ~TransformComponent() = default;

        void SetParent(TransformComponent*);

    private:
        TransformComponent* parent;
        std::vector<TransformComponent*> children;
        Transform local;
        Transform global;
    };

}