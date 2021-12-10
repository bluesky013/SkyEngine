//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <framework/environment/Singleton.h>
#include <framework/serialization/SerializationFactory.h>

namespace sky {

    class SerializationContext : public Singleton<SerializationContext> {
    public:
        template <typename T>
        auto Register(const std::string_view &key)
        {
            auto& type = types[key];
            type.info = TypeInfoObj<T>::Get()->RtInfo();
            type.info->typeId = key;
            return TypeFactory<T>(type);
        }

        TypeNode* FindType(const std::string& key);

    private:
        friend class Singleton<SerializationContext>;

        SerializationContext() = default;
        ~SerializationContext() = default;

        std::unordered_map<std::string_view, TypeNode> types;
    };

    template <typename T, typename ...Args>
    inline Any MakeAny(Args&&... args)
    {
        auto context = SerializationContext::Get();
        auto rtInfo = TypeInfoObj<T>::Get()->RtInfo();
        if(rtInfo == nullptr) {
            return {};
        }
        TypeNode* node = context->FindType(rtInfo->typeId.data());
        if (node->constructList.empty()) {
            return {};
        }
        std::array<Any, sizeof...(Args)> anyArgs{std::forward<Args>(args)...};
        return node->constructList.back().constructFn(anyArgs.data());
    }

}
