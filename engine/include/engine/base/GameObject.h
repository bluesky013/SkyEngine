//
// Created by Zach Lee on 2021/11/12.
//

#pragma once

#include <core/type/Rtti.h>
#include <core/std/Container.h>
#include <engine/base/Component.h>
#include <engine/base/Object.h>
#include <list>
#include <string>

namespace sky {
    class World;

    class GameObject : public Object {
    public:
        GameObject(GameObject &&) noexcept = default;
        GameObject &operator=(GameObject&&) noexcept = default;

        GameObject(const GameObject &)            = delete;
        GameObject &operator=(const GameObject &) = delete;

        template <typename T>
        inline T *AddComponent()
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component *comp) { return comp->GetTypeInfo() == info; });
            if (iter == components.end()) {
                auto comp    = ComponentFactory<T>::CreateComponent(resource->allocate(sizeof(T)));
                comp->object = this;
                comp->OnActive();
                ComponentFactory<T>::Get()->template ForEach<&IComponentListener::OnAddComponent>(this, comp);
                components.emplace_back(comp);
                return comp;
            }
            return static_cast<T *>(*iter);
        }

        template <typename T>
        inline void RemoveComponent()
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component *comp) { return comp->GetTypeInfo() == info; });
            if (iter != components.end()) {
                ComponentFactory<T>::Get()->template ForEach<&IComponentListener::OnRemoveComponent>(this, *iter);
                (*iter)->OnDestroy();
                (*iter)->~Component();
                resource->deallocate(*iter, info->size);
                components.erase(iter);
            }
        }

        template <typename T>
        inline T *GetComponent()
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component *comp) {
                return comp->GetTypeInfo() == info;
            });
            if (iter != components.end()) {
                return static_cast<T *>(*iter);
            }
            return nullptr;
        }

        template <typename T>
        inline const T *GetComponent() const
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component *comp) { return comp->GetTypeInfo() == info; });
            if (iter != components.end()) {
                return static_cast<T *>(*iter);
            }
            return nullptr;
        }

        uint32_t GetId() const;

        const std::string &GetName() const;

        World *GetWorld() const;

        void SetParent(GameObject *gameObject);

        void Tick(float time);

        using ComponentList = PmrList<Component *>;
        ComponentList &GetComponents();

    private:
        friend class World;
        ~GameObject();
        GameObject(const std::string &str) : name(str)
        {
        }

        World        *world = nullptr;
        PmrResource  *resource = nullptr;
        uint32_t      objId = 0;
        std::string   name;
        ComponentList components;
    };

} // namespace sky
