//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <vector>

namespace sky {

    class Component;
    class World;

    class GameObject {
    public:
        ~GameObject();

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;

        template <typename T, typename ...Args>
        T* AddComponent(Args&& ...args)
        {
            AddComponent(new T(std::move(std::forward<Args>(args))...));
        }

        void AddComponent(Component* component);

        void RemoveComponent(Component* component);

    private:
        friend class World;
        GameObject() = default;
        World* world = nullptr;
        std::vector<Component*> components;
    };

}