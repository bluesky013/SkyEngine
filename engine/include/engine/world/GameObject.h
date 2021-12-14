//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <string>
#include <map>
#include <engine/world/Component.h>

namespace sky {
    class World;

    class GameObject {
    public:
        ~GameObject();

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;

        template <typename T>
        inline T* AddComponent()
        {
            auto comp = ComponentBuilder<T>::CreateComponent();
            comp->object = this;
            components.emplace(ComponentBuilder<T>::id, comp);
            return comp;
        }

        template <typename T>
        inline void RemoveComponent()
        {
            auto iter = components.find(ComponentBuilder<T>::id);
            if (iter != components.end()) {
                delete iter->second;
                components.erase(iter);
            }
        }

        template <typename T>
        inline T* GetComponent()
        {
            auto iter = components.find(ComponentBuilder<T>::id);
            if (iter != components.end()) {
                return static_cast<T*>(iter->second);
            }
            return nullptr;
        }

        template <typename T>
        inline const T* GetComponent() const
        {
            auto iter = components.find(ComponentBuilder<T>::id);
            if (iter != components.end()) {
                return static_cast<T*>(iter->second);
            }
            return nullptr;
        }

        uint32_t GetId() const;

        const std::string& GetName() const;

        void SetParent(GameObject* gameObject);

        using ComponentMap = std::map<uint32_t, Component*>;
        ComponentMap& GetComponents();

    private:
        friend class World;
        GameObject(const std::string& str) : name(str) {}
        World* world = nullptr;
        uint32_t objId = 0;
        std::string name;
        ComponentMap components;
    };

}