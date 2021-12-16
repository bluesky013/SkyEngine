//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <string>
#include <list>
#include <engine/world/Component.h>
#include <core/util/Rtti.h>

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
            components.emplace_back(comp);
            return comp;
        }

        template <typename T>
        inline void RemoveComponent()
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component* comp) {
                return comp->GetTypeInfo() == info;
            });
            if (iter != components.end()) {
                delete *iter;
                components.erase(iter);
            }
        }

        template <typename T>
        inline T* GetComponent()
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component* comp) {
                auto ci = comp->GetTypeInfo();
                return comp->GetTypeInfo() == info;
            });
            if (iter != components.end()) {
                return static_cast<T*>(*iter);
            }
            return nullptr;
        }

        template <typename T>
        inline const T* GetComponent() const
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component* comp) {
                return comp->GetTypeInfo() == info;
            });
            if (iter != components.end()) {
                return static_cast<T*>(*iter);
            }
            return nullptr;
        }

        uint32_t GetId() const;

        const std::string& GetName() const;

        void SetParent(GameObject* gameObject);

        using ComponentList = std::list<Component*>;
        ComponentList& GetComponents();

    private:
        friend class World;
        GameObject(const std::string& str) : name(str) {}
        World* world = nullptr;
        uint32_t objId = 0;
        std::string name;
        ComponentList components;
    };

}