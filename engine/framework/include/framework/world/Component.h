//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/memory/Allocator.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {
    class Actor;
    class World;
    class JsonOutputArchive;
    class JsonInputArchive;
    enum class ComponentPreparation : uint8_t { Preparing, Ready, Failed };

    struct ComponentAssetReference {
        Uuid        uuid;
        std::string type;
    };

    class ComponentBase {
    public:
        ComponentBase()          = default;
        virtual ~ComponentBase() = default;

        virtual void Tick(float time)
        {
        }
        virtual void PostTick(float time)
        {
        }

        virtual void SaveJson(JsonOutputArchive &archive) const;
        virtual void LoadJson(JsonInputArchive &archive);
        virtual void CollectAssetReferences(std::vector<ComponentAssetReference> &out) const;
        // Called on detached components by the host. Cancellation must not invalidate shared work.
        virtual ComponentPreparation PrepareForWorld(World &world, std::string &error)
        {
            return ComponentPreparation::Ready;
        }
        virtual void CancelPreparation()
        {
        }
        virtual bool KeepWorldResidency() const
        {
            return false;
        }

        virtual void SaveBin(BinaryOutputArchive &archive) const
        {
        }
        virtual void LoadBin(BinaryInputArchive &archive)
        {
        }

        virtual const Uuid &GetTypeId() const = 0;
        // The concrete macro returns the complete object address for its registered reflection type.
        virtual void *GetReflectedObject()
        {
            return nullptr;
        }

        // On failure Actor calls OnDetachFromWorld to release any partial attachment.
        virtual bool TryAttachToWorld(std::string &error)
        {
            OnAttachToWorld();
            return true;
        }
        virtual void OnAttachToWorld()
        {
        }
        virtual void OnDetachFromWorld()
        {
        }

        virtual void OnSerialized()
        {
        }

        Actor *GetActor() const
        {
            return actor;
        }

    protected:
        friend class Actor;
        Actor *actor = nullptr;
    };

    template <typename Data>
    class ComponentAdaptor : public ComponentBase {
    public:
        ComponentAdaptor()           = default;
        ~ComponentAdaptor() override = default;

        void SaveJson(JsonOutputArchive &archive) const override
        {
            archive.SaveValueObject(data);
        }

        void LoadJson(JsonInputArchive &archive) override
        {
            archive.LoadValueObject(data);
        }

        const Data &GetData() const
        {
            return data;
        }

    protected:
        Data data = {};
    };

#define COMPONENT_RUNTIME_INFO(NAME)                                                                                                                 \
    using MY_CLASS = NAME;                                                                                                                           \
    void *GetReflectedObject() override                                                                                                              \
    {                                                                                                                                                \
        return this;                                                                                                                                 \
    }                                                                                                                                                \
    const Uuid &GetTypeId() const override                                                                                                           \
    {                                                                                                                                                \
        return TypeInfoObj<MY_CLASS>::Get()->RtInfo()->registeredId;                                                                                 \
    }
} // namespace sky
