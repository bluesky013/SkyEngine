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

        void AddComponent(Component* component);

        void RemoveComponent(Component* component);

        template <typename T, typename ...Args>
        T* AddComponent(Args&& ...args)
        {
            auto comp = new T(std::move(std::forward<Args>(args))...);
            AddComponent(comp);
            return comp;
        }

        uint32_t GetId() const;

    private:
        friend class World;
        GameObject() = default;
        World* world = nullptr;
        uint32_t objId = 0;
        std::vector<Component*> components;
    };

}