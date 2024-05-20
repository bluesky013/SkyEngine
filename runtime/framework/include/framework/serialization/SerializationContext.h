//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <array>
#include <core/environment/Singleton.h>
#include <core/platform/Platform.h>
#include <framework/serialization/SerializationFactory.h>

namespace sky {

    class SerializationContext : public Singleton<SerializationContext> {
    public:
        template <typename T>
        auto Register(std::string_view name, const Uuid &uuid)
        {
            const auto *info = TypeInfoObj<T>::Get()->Register(name, uuid);
            SKY_ASSERT(!types.count(info->registeredId))
            auto &type = types[info->registeredId];
            type.info  = info;
            if constexpr (std::is_default_constructible_v<T>) {
                type.constructList.emplace_back(
                        ConstructNode{0,
                                      [](Any *args) { return true; },
                                      [](Any *args) -> Any { return Any(std::in_place_type<T>); }});
            }

            SKY_ASSERT(lookupTable.emplace(info->name, &type).second)
            return TypeFactory<T>(type);
        }

        template <typename T>
        auto Register(std::string_view name)
        {
            return Register<T>(name, Uuid::CreateWithSeed(Fnv1a32(name)));
        }

        TypeNode *FindType(const std::string &key);
        TypeNode *FindTypeById(const Uuid &id);

    private:
        friend class Singleton<SerializationContext>;

        SerializationContext();
        ~SerializationContext() override = default;

        std::unordered_map<Uuid, TypeNode>  types;
        std::unordered_map<std::string_view, TypeNode*> lookupTable;
    };

    template <typename... Args>
    Any MakeAny(const Uuid &typeId, Args &&...args)
    {
        auto *context = SerializationContext::Get();
        TypeNode *node = context->FindTypeById(typeId);
        if (node == nullptr || node->constructList.empty()) {
            return {};
        }
        for (auto &ctr : node->constructList) {
            std::array<Any, sizeof...(Args)> anyArgs{Any(std::in_place_type<Args>, std::forward<Args>(args))...};
            if (ctr.argsNum == sizeof...(Args) && ctr.checkFn(anyArgs.data())) {
                return ctr.constructFn(anyArgs.data());
            }
        }

        return {};
    }

    template <typename T, typename... Args>
    inline Any MakeAny(Args &&...args)
    {
        return MakeAny(TypeInfo<T>::RegisteredId(), std::forward<Args>(args)...);
    }

    inline const TypeNode *GetTypeNode(const Uuid &typeId)
    {
        auto *context = SerializationContext::Get();
        return context->FindTypeById(typeId);
    }

    inline const TypeNode *GetTypeNode(const Any &any)
    {
        const auto *rtInfo = any.Info();
        if (rtInfo == nullptr) {
            return nullptr;
        }
        auto *context = SerializationContext::Get();
        return context->FindTypeById(rtInfo->registeredId);
    }

    inline const TypeNode *GetTypeNode(const TypeInfoRT *rtInfo)
    {
        if (rtInfo == nullptr) {
            return nullptr;
        }
        auto *context = SerializationContext::Get();
        return context->FindTypeById(rtInfo->registeredId);
    }

    inline TypeMemberNode *GetTypeMember(const std::string_view &member, const Uuid &typeId)
    {
        auto *context = SerializationContext::Get();
        auto *node = context->FindTypeById(typeId);
        if (node == nullptr) {
            return nullptr;
        }

        auto it = node->members.find(member);
        if (it == node->members.end()) {
            return nullptr;
        }

        return &it->second;
    }

    enum class SerializeOption : uint8_t { BIN, JSON };

} // namespace sky

#define REGISTER_BEGIN(NAME, context) context->Register<MY_CLASS>(#NAME)
#define REGISTER_MEMBER(NAME, Setter, Getter) .Member<&MY_CLASS::Setter, &MY_CLASS::Getter>(#NAME)

