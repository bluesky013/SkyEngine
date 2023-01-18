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
        auto Register(const std::string_view &key)
        {
            auto info = TypeInfoObj<T>::Get()->RtInfo();
            SKY_ASSERT(!types.count(info->typeId))
            auto &type      = types[info->typeId];
            type.info       = info;
            SKY_ASSERT(lookupTable.emplace(key, &type).second)
            return TypeFactory<T>(type);
        }

        TypeNode *FindType(const std::string &key);
        TypeNode *FindTypeById(uint32_t id);

    private:
        friend class Singleton<SerializationContext>;

        SerializationContext()  = default;
        ~SerializationContext() = default;

        std::unordered_map<uint32_t, TypeNode>  types;
        std::unordered_map<std::string_view, TypeNode*> lookupTable;
    };

    template <typename T, typename... Args>
    inline Any MakeAny(Args &&...args)
    {
        auto context = SerializationContext::Get();
        auto rtInfo  = TypeInfoObj<T>::Get()->RtInfo();
        if (rtInfo == nullptr) {
            return {};
        }
        TypeNode *node = context->FindTypeById(rtInfo->typeId);
        if (node == nullptr || node->constructList.empty()) {
            return {};
        }
        std::array<Any, sizeof...(Args)> anyArgs{std::forward<Args>(args)...};
        return node->constructList.back().constructFn(anyArgs.data());
    }

    inline const TypeNode *GetTypeNode(const Any &any)
    {
        auto rtInfo = any.Info();
        if (rtInfo == nullptr) {
            return nullptr;
        }
        auto context = SerializationContext::Get();
        return context->FindTypeById(rtInfo->typeId);
    }

    inline const TypeNode *GetTypeNode(const TypeInfoRT *rtInfo)
    {
        if (rtInfo == nullptr) {
            return nullptr;
        }
        auto context = SerializationContext::Get();
        return context->FindTypeById(rtInfo->typeId);
    }

    inline TypeMemberNode *GetTypeMember(const std::string &str, const TypeInfoRT *info)
    {
        if (info == nullptr) {
            return nullptr;
        }
        auto context = SerializationContext::Get();
        auto node    = context->FindTypeById(info->typeId);
        if (node == nullptr) {
            return nullptr;
        }

        auto it = node->members.find(str);
        if (it == node->members.end()) {
            return nullptr;
        }

        return &it->second;
    }

    enum class SerializeOption : uint8_t { BIN, JSON };

} // namespace sky

#define TYPE_RTTI_BASE virtual const sky::TypeInfoRT *GetTypeInfo() const = 0;

#define TYPE_RTTI_WITH_VT(name, uuid)                                                                                                                \
    const sky::TypeInfoRT *GetTypeInfo() const override                                                                                              \
    {                                                                                                                                                \
        static const sky::TypeInfoRT *info = sky::TypeInfoObj<name>::Get()->RtInfo();                                                                \
        return info;                                                                                                                                 \
    }                                                                                                                                                \
    TYPE_RTTI(name)                                                                                                                                  \
    static constexpr std::string_view S_TYPE = uuid;                                                                                                 \
    static constexpr sky::Uuid TYPE = sky::Uuid::CreateFromString(uuid);                                                                             \
    sky::Uuid GetType() const override                                                                                                               \
    {                                                                                                                                                \
        return TYPE;                                                                                                                                 \
    }                                                                                                                                                \
    std::string_view GetTypeStr() const override                                                                                                     \
    {                                                                                                                                                \
        return S_TYPE;                                                                                                                               \
    }


