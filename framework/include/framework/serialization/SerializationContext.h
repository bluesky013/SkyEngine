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

        TypeNode* FindType(const std::string& key)
        {
            auto iter = types.find(key);
            if (iter == types.end()) {
                return nullptr;
            }
            return &(iter->second);
        }

    private:
        friend class Singleton<SerializationContext>;

        SerializationContext() = default;
        ~SerializationContext() = default;

        std::unordered_map<std::string_view, TypeNode> types;
    };

}
