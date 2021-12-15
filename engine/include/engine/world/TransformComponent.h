//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <core/math/Transform.h>
#include <engine/world/Component.h>
#include <vector>

namespace sky {

    class TransformComponent : public Component {
    public:
        TransformComponent() = default;
        ~TransformComponent();

        TYPE_RTTI_WITH_VT(TransformComponent)

        static void Reflect();

        void SetParent(TransformComponent*);

        TransformComponent* GetParent() const;

        const std::vector<TransformComponent*>& GetChildren() const;

        void Update();

        void Print();

        Transform local;
        Transform global;
    private:
        static void PrintChild(TransformComponent& comp, std::string str);

        TransformComponent* parent = nullptr;
        std::vector<TransformComponent*> children;
    };

}