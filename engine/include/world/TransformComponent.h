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
        ~TransformComponent();

        void SetParent(TransformComponent*);

        TransformComponent* GetParent() const;

        const std::vector<TransformComponent*>& GetChildren() const;

        void Update();

        void Print();

    private:
        static void PrintChild(TransformComponent& comp, std::string str);

        TransformComponent* parent = nullptr;
        std::vector<TransformComponent*> children;
        Transform local;
        Transform global;
    };

}