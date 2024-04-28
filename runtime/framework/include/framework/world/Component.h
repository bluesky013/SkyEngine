//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/memory/Allocator.h>
#include <framework/world/ComponentFactory.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/BinaryArchive.h>

namespace sky {
    class Actor;
    class JsonOutputArchive;
    class JsonInputArchive;

    class ComponentBase {
    public:
        ComponentBase()          = default;
        virtual ~ComponentBase() = default;

        virtual void Tick(float time) {}

        virtual void OnActive() {}
        virtual void OnDeActive() {}

        virtual void SaveJson(JsonOutputArchive &archive) const {}
        virtual void LoadJson(JsonInputArchive &archive) {}

    protected:
        friend class Actor;
        Actor *actor = nullptr;
    };

    template <typename Data>
    class ComponentAdaptor : public ComponentBase {
    public:
        ComponentAdaptor() = default;
        ~ComponentAdaptor() override = default;

        void SaveJson(JsonOutputArchive &archive) const override
        {
            archive.SaveValueObject(data);
        }

        void LoadJson(JsonInputArchive &archive) override
        {
            archive.LoadValueObject(data);
        }

        const Data &GetData() const { return data; }

    protected:
        Data data = {};
    };


#define COMPONENT_RUNTIME_INFO(NAME) \
    using MY_CLASS = NAME;

} // namespace sky
