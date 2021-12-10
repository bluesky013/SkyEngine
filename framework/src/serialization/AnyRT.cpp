//
// Created by Zach Lee on 2021/12/11.
//

#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    TypeMemberNode* GetTypeMember(const std::string& str, TypeInfoRT* info)
    {
        if(info == nullptr) {
            return nullptr;
        }
        auto context = SerializationContext::Get();
        auto node = context->FindType(info->typeId.data());
        if (node == nullptr) {
            return nullptr;
        }

        auto it = node->members.find(str);
        if (it == node->members.end()) {
            return nullptr;
        }

        return &it->second;
    }

    bool Any::Set(const std::string& str, const Any& any)
    {
        auto node = GetTypeMember(str, info);
        if (node->setterFn != nullptr) {
            return node->setterFn(*this, any);
        }
        return false;
    }

    Any Any::Get(const std::string& str)
    {
        auto node = GetTypeMember(str, info);
        if (node->getterFn != nullptr) {
            return node->getterFn(*this);
        }
        return Any{};
    }

}