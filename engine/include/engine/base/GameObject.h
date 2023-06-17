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

    class JsonOutputArchive;
    class JsonInputArchive;

    class GameObject : public Object {
    public:
        GameObject(GameObject &&) noexcept = default;
        GameObject &operator=(GameObject&&) noexcept = default;

        GameObject(const GameObject &)            = delete;
        GameObject &operator=(const GameObject &) = delete;

        static void Reflect();

        inline Component *AddComponent(uint32_t id)
        {
            auto iter = std::find_if(components.begin(), components.end(), [&id](Component *comp) { return comp->GetType() == id; });
            if (iter != components.end()) {
                return *iter;
            }

            auto comp = ComponentFactory::Get()->CreateComponent(resource, id);
            if (comp != nullptr) {
                comp->object = this;
                comp->OnActive();
                ComponentFactory::Get()->template ForEach<&IComponentListener::OnAddComponent>(this, comp);
                components.emplace_back(comp);
            }
            return comp;
        }

        template <typename T>
        inline T *AddComponent()
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component *comp) { return comp->GetTypeInfo() == info; });
            if (iter == components.end()) {
                auto comp    = ComponentFactory::CreateComponent<T>(resource);
                comp->object = this;
                comp->OnActive();
                ComponentFactory::Get()->template ForEach<&IComponentListener::OnAddComponent>(this, comp);
                components.emplace_back(comp);
                return static_cast<T*>(comp);
            }
            return static_cast<T *>(*iter);
        }

        template <typename T>
        inline void RemoveComponent()
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            auto iter = std::find_if(components.begin(), components.end(), [info](Component *comp) { return comp->GetTypeInfo() == info; });
            if (iter != components.end()) {
                ComponentFactory::Get()->template ForEach<&IComponentListener::OnRemoveComponent>(this, *iter);
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

        void SetName(const std::string &name);
        const std::string &GetName() const;

        World *GetWorld() const;

        void SetParent(GameObject *gameObject);
        void SetParent(const Uuid &gameObject);

        GameObject *GetParent() const;

        void Tick(float time);

        using ComponentList = PmrList<Component *>;
        ComponentList &GetComponents();

        void Save(JsonOutputArchive &ar) const;
        void Load(JsonInputArchive &ar);

    private:
        friend class World;
        ~GameObject();
        GameObject() = default;
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
