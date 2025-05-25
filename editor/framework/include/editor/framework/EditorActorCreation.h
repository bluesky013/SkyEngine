//
// Created by blues on 2025/5/18.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/name/Name.h>
#include <list>

namespace sky {
    class Actor;
} // namespace sky

namespace sky::editor {

    class IActorCreateBase {
    public:
        IActorCreateBase() = default;
        virtual ~IActorCreateBase() = default;

        virtual Name GetGroup() const = 0;
        virtual Name GetName() const = 0;

        virtual bool OnCreateActor(Actor* actor) = 0;
    };

    class EditorActorCreation : public Singleton<EditorActorCreation> {
    public:
        EditorActorCreation() = default;
        ~EditorActorCreation() override = default;

        void RegisterCreation(IActorCreateBase* tool);
        void UnRegisterCreation(IActorCreateBase* tool);

        const std::list<IActorCreateBase*> &GetTools() const
        {
            return creatFn;
        }

    private:
        std::list<IActorCreateBase*> creatFn;
    };

} // namespace sky::editor
